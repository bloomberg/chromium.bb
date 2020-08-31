// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/thin_webview/internal/thin_webview.h"

#include "base/android/jni_android.h"
#include "cc/layers/layer.h"
#include "chrome/browser/android/thin_webview/internal/jni_headers/ThinWebViewImpl_jni.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_controls_state.h"

using base::android::JavaParamRef;
using web_contents_delegate_android::WebContentsDelegateAndroid;

namespace thin_webview {
namespace android {

jlong JNI_ThinWebViewImpl_Init(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               const JavaParamRef<jobject>& jcompositor_view,
                               const JavaParamRef<jobject>& jwindow_android) {
  CompositorView* compositor_view =
      CompositorViewImpl::FromJavaObject(jcompositor_view);
  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  ThinWebView* view =
      new ThinWebView(env, obj, compositor_view, window_android);
  return reinterpret_cast<intptr_t>(view);
}

ThinWebView::ThinWebView(JNIEnv* env,
                         jobject obj,
                         CompositorView* compositor_view,
                         ui::WindowAndroid* window_android)
    : obj_(env, obj),
      compositor_view_(compositor_view),
      window_android_(window_android),
      web_contents_(nullptr) {}

ThinWebView::~ThinWebView() {}

void ThinWebView::Destroy(JNIEnv* env, const JavaParamRef<jobject>& object) {
  delete this;
}

void ThinWebView::SetWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jweb_contents_delegate) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  WebContentsDelegateAndroid* delegate =
      jweb_contents_delegate.is_null()
          ? nullptr
          : new WebContentsDelegateAndroid(env, jweb_contents_delegate);
  SetWebContents(web_contents, delegate);
}

void ThinWebView::SetWebContents(content::WebContents* web_contents,
                                 WebContentsDelegateAndroid* delegate) {
  DCHECK(web_contents);
  web_contents_ = web_contents;
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  if (view_android->parent() != window_android_) {
    window_android_->AddChild(view_android);
  }

  compositor_view_->SetRootLayer(web_contents_->GetNativeView()->GetLayer());
  ResizeWebContents(view_size_);
  web_contents_delegate_.reset(delegate);
  if (delegate)
    web_contents->SetDelegate(delegate);

  TabHelpers::AttachTabHelpers(web_contents);
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_web_contents_supports_permission_requests(false);
  ViewAndroidHelper::FromWebContents(web_contents)
      ->SetViewAndroid(web_contents->GetNativeView());

  // Disable browser controls when used for thin webview.
  web_contents->GetMainFrame()->UpdateBrowserControlsState(
      content::BROWSER_CONTROLS_STATE_HIDDEN,
      content::BROWSER_CONTROLS_STATE_HIDDEN, false);
}

void ThinWebView::SizeChanged(JNIEnv* env,
                              const JavaParamRef<jobject>& object,
                              jint width,
                              jint height) {
  view_size_ = gfx::Size(width, height);

  // TODO(shaktisahu): If we want to use a different size for WebContents, e.g.
  // showing full screen contents instead inside this view, don't do the resize.
  if (web_contents_)
    ResizeWebContents(view_size_);
}

void ThinWebView::ResizeWebContents(const gfx::Size& size) {
  if (!web_contents_)
    return;

  web_contents_->GetNativeView()->OnPhysicalBackingSizeChanged(size);
  web_contents_->GetNativeView()->OnSizeChanged(size.width(), size.height());
}

}  // namespace android
}  // namespace thin_webview
