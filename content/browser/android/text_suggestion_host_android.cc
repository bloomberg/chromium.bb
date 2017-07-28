// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/text_suggestion_host_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/browser/android/text_suggestion_host_mojo_impl_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/TextSuggestionHost_jni.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/gfx/android/view_configuration.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace content {

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  auto* text_suggestion_host =
      new TextSuggestionHostAndroid(env, obj, web_contents);
  text_suggestion_host->Initialize();
  return reinterpret_cast<intptr_t>(text_suggestion_host);
}

TextSuggestionHostAndroid::TextSuggestionHostAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    WebContents* web_contents)
    : RenderWidgetHostConnector(web_contents),
      WebContentsObserver(web_contents),
      rwhva_(nullptr),
      java_text_suggestion_host_(JavaObjectWeakGlobalRef(env, obj)),
      spellcheck_menu_timeout_(
          base::Bind(&TextSuggestionHostAndroid::OnSpellCheckMenuTimeout,
                     base::Unretained(this))) {
  registry_.AddInterface(base::Bind(&TextSuggestionHostMojoImplAndroid::Create,
                                    base::Unretained(this)));
}

TextSuggestionHostAndroid::~TextSuggestionHostAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_text_suggestion_host_.get(env);
  if (!obj.is_null())
    Java_TextSuggestionHost_destroy(env, obj);
}

void TextSuggestionHostAndroid::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  text_suggestion_backend_ = nullptr;
  if (old_rwhva)
    old_rwhva->set_text_suggestion_host(nullptr);
  if (new_rwhva)
    new_rwhva->set_text_suggestion_host(this);
  rwhva_ = new_rwhva;
}

void TextSuggestionHostAndroid::ApplySpellCheckSuggestion(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jstring>& replacement) {
  const blink::mojom::TextSuggestionBackendPtr& text_suggestion_backend =
      GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->ApplySpellCheckSuggestion(
      ConvertJavaStringToUTF8(env, replacement));
}

void TextSuggestionHostAndroid::DeleteActiveSuggestionRange(
    JNIEnv*,
    const JavaParamRef<jobject>&) {
  const blink::mojom::TextSuggestionBackendPtr& text_suggestion_backend =
      GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->DeleteActiveSuggestionRange();
}

void TextSuggestionHostAndroid::NewWordAddedToDictionary(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jstring>& word) {
  const blink::mojom::TextSuggestionBackendPtr& text_suggestion_backend =
      GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->NewWordAddedToDictionary(
      ConvertJavaStringToUTF8(env, word));
}

void TextSuggestionHostAndroid::SuggestionMenuClosed(
    JNIEnv*,
    const JavaParamRef<jobject>&) {
  const blink::mojom::TextSuggestionBackendPtr& text_suggestion_backend =
      GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->SuggestionMenuClosed();
}

void TextSuggestionHostAndroid::ShowSpellCheckSuggestionMenu(
    double caret_x,
    double caret_y,
    const std::string& marked_text,
    const std::vector<blink::mojom::SpellCheckSuggestionPtr>& suggestions) {
  std::vector<std::string> suggestion_strings;
  for (const auto& suggestion_ptr : suggestions)
    suggestion_strings.push_back(suggestion_ptr->suggestion);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_text_suggestion_host_.get(env);
  if (obj.is_null())
    return;

  Java_TextSuggestionHost_showSpellCheckSuggestionMenu(
      env, obj, caret_x, caret_y, ConvertUTF8ToJavaString(env, marked_text),
      ToJavaArrayOfStrings(env, suggestion_strings));
}

void TextSuggestionHostAndroid::StartSpellCheckMenuTimer() {
  spellcheck_menu_timeout_.Stop();
  spellcheck_menu_timeout_.Start(base::TimeDelta::FromMilliseconds(
      gfx::ViewConfiguration::GetDoubleTapTimeoutInMs()));
}

void TextSuggestionHostAndroid::OnKeyEvent() {
  spellcheck_menu_timeout_.Stop();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_text_suggestion_host_.get(env);
  if (obj.is_null())
    return;

  Java_TextSuggestionHost_hidePopups(env, obj);
}

void TextSuggestionHostAndroid::StopSpellCheckMenuTimer() {
  spellcheck_menu_timeout_.Stop();
}

void TextSuggestionHostAndroid::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

RenderFrameHost* TextSuggestionHostAndroid::GetFocusedFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We get the focused frame from the WebContents of the page. Although
  // |rwhva_->GetFocusedWidget()| does a similar thing, there is no direct way
  // to get a RenderFrameHost from its RWH.
  if (!rwhva_)
    return nullptr;
  RenderWidgetHostImpl* rwh =
      RenderWidgetHostImpl::From(rwhva_->GetRenderWidgetHost());
  if (!rwh || !rwh->delegate())
    return nullptr;

  if (auto* contents = rwh->delegate()->GetAsWebContents())
    return contents->GetFocusedFrame();

  return nullptr;
}

const blink::mojom::TextSuggestionBackendPtr&
TextSuggestionHostAndroid::GetTextSuggestionBackend() {
  if (!text_suggestion_backend_) {
    if (RenderFrameHost* rfh = GetFocusedFrame()) {
      rfh->GetRemoteInterfaces()->GetInterface(
          mojo::MakeRequest(&text_suggestion_backend_));
    }
  }
  return text_suggestion_backend_;
}

void TextSuggestionHostAndroid::OnSpellCheckMenuTimeout() {
  const blink::mojom::TextSuggestionBackendPtr& text_suggestion_backend =
      GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->SpellCheckMenuTimeoutCallback();
}

}  // namespace content
