// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_content_window_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "ipc/ipc_message.h"
#include "jni/CastContentWindowAndroid_jni.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"

namespace chromecast {
namespace shell {

namespace {
base::android::ScopedJavaLocalRef<jobject> CreateJavaWindow(
    jlong nativeWindow) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CastContentWindowAndroid_create(env, nativeWindow);
}
}  // namespace

// static
bool CastContentWindowAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    CastContentWindow::Delegate* delegate) {
  return base::WrapUnique(new CastContentWindowAndroid(delegate));
}

CastContentWindowAndroid::CastContentWindowAndroid(
    CastContentWindow::Delegate* delegate)
    : delegate_(delegate),
      transparent_(false),
      java_window_(CreateJavaWindow(reinterpret_cast<jlong>(this))) {
  DCHECK(delegate_);
}

CastContentWindowAndroid::~CastContentWindowAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_onNativeDestroyed(env, java_window_.obj());
}

void CastContentWindowAndroid::SetTransparent() {
  transparent_ = true;
}

void CastContentWindowAndroid::ShowWebContents(
    content::WebContents* web_contents,
    CastWindowManager* window_manager) {
  DCHECK(window_manager);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      web_contents->GetJavaWebContents();

  Java_CastContentWindowAndroid_showWebContents(env, java_window_.obj(),
                                                java_web_contents.obj());
}

std::unique_ptr<content::WebContents>
CastContentWindowAndroid::CreateWebContents(
    content::BrowserContext* browser_context) {
  CHECK(display::Screen::GetScreen());
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();

  content::WebContents::CreateParams create_params(browser_context, nullptr);
  create_params.routing_id = MSG_ROUTING_NONE;
  create_params.initial_size = display_size;
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(create_params));

  content::RendererPreferences* prefs = web_contents->GetMutableRendererPrefs();
  prefs->use_video_overlay_for_embedded_encrypted_video = true;
  web_contents->GetRenderViewHost()->SyncRendererPrefs();

  content::WebContentsObserver::Observe(web_contents.get());
  return web_contents;
}

void CastContentWindowAndroid::DidFirstVisuallyNonEmptyPaint() {
  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstPaint();
}

void CastContentWindowAndroid::MediaStartedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPlay();
}

void CastContentWindowAndroid::MediaStoppedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPause();
}

void CastContentWindowAndroid::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::RenderWidgetHostView* view =
      render_view_host->GetWidget()->GetView();
  if (view) {
    view->SetBackgroundColor(transparent_ ? SK_ColorTRANSPARENT
                                          : SK_ColorBLACK);
  }
}

void CastContentWindowAndroid::OnActivityStopped(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  delegate_->OnWindowDestroyed();
}

void CastContentWindowAndroid::OnKeyDown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int keycode) {
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED,
                         ui::KeyboardCodeFromAndroidKeyCode(keycode),
                         ui::EF_NONE);
  delegate_->OnKeyEvent(key_event);
}

}  // namespace shell
}  // namespace chromecast
