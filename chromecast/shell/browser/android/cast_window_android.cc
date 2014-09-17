// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/android/cast_window_android.h"

#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "chromecast/shell/browser/android/cast_window_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/renderer_preferences.h"
#include "jni/CastWindowAndroid_jni.h"

namespace chromecast {
namespace shell {

// static
bool CastWindowAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

CastWindowAndroid::CastWindowAndroid(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

CastWindowAndroid::~CastWindowAndroid() {
}

// static
CastWindowAndroid* CastWindowAndroid::CreateNewWindow(
    content::BrowserContext* browser_context,
    const GURL& url) {
  content::WebContents::CreateParams create_params(browser_context);
  create_params.routing_id = MSG_ROUTING_NONE;
  content::WebContents* web_contents =
      content::WebContents::Create(create_params);
  CastWindowAndroid* shell = CreateCastWindowAndroid(
      web_contents,
      create_params.initial_size);
  if (!url.is_empty())
    shell->LoadURL(url);
  return shell;
}

// static
CastWindowAndroid* CastWindowAndroid::CreateCastWindowAndroid(
    content::WebContents* web_contents,
    const gfx::Size& initial_size) {
  CastWindowAndroid* shell = new CastWindowAndroid(web_contents);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> shell_android(
      CreateCastWindowView(shell));

  shell->java_object_.Reset(env, shell_android.Release());
  shell->web_contents_.reset(web_contents);
  web_contents->SetDelegate(shell);

  Java_CastWindowAndroid_initFromNativeWebContents(
      env, shell->java_object_.obj(), reinterpret_cast<jint>(web_contents));

  // Enabling hole-punching also requires runtime renderer preference
  web_contents->GetMutableRendererPrefs()->
      use_video_overlay_for_embedded_encrypted_video = true;
  web_contents->GetRenderViewHost()->SyncRendererPrefs();

  return shell;
}

void CastWindowAndroid::Close() {
  // Note: if multiple windows becomes supported, this may close other devtools
  // sessions.
  content::DevToolsAgentHost::DetachAllClients();
  CloseCastWindowView(java_object_.obj());
  delete this;
}

void CastWindowAndroid::LoadURL(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = content::PageTransitionFromInt(
      content::PAGE_TRANSITION_TYPED |
      content::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void CastWindowAndroid::AddNewContents(content::WebContents* source,
                                       content::WebContents* new_contents,
                                       WindowOpenDisposition disposition,
                                       const gfx::Rect& initial_pos,
                                       bool user_gesture,
                                       bool* was_blocked) {
  NOTIMPLEMENTED();
  if (was_blocked) {
    *was_blocked = true;
  }
}

void CastWindowAndroid::CloseContents(content::WebContents* source) {
  DCHECK_EQ(source, web_contents_.get());
  Close();
}

bool CastWindowAndroid::CanOverscrollContent() const {
  return false;
}

bool CastWindowAndroid::AddMessageToConsole(content::WebContents* source,
                                            int32 level,
                                            const base::string16& message,
                                            int32 line_no,
                                            const base::string16& source_id) {
  return false;
}

void CastWindowAndroid::ActivateContents(content::WebContents* contents) {
  DCHECK_EQ(contents, web_contents_.get());
  contents->GetRenderViewHost()->Focus();
}

void CastWindowAndroid::DeactivateContents(content::WebContents* contents) {
  DCHECK_EQ(contents, web_contents_.get());
  contents->GetRenderViewHost()->Blur();
}

void CastWindowAndroid::RenderProcessGone(base::TerminationStatus status) {
  LOG(ERROR) << "Render process gone: status=" << status;
  Close();
}

}  // namespace shell
}  // namespace chromecast
