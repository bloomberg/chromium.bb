// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/aw_browser_dependency_factory_impl.h"

// TODO(joth): Componentize or remove chrome/... dependencies.
#include "android_webview/native/aw_contents_container.h"
#include "base/lazy_instance.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_creator.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"

namespace android_webview {

namespace {

base::LazyInstance<AwBrowserDependencyFactoryImpl>::Leaky g_lazy_instance;

class TabContentsWrapper : public AwContentsContainer {
 public:
  TabContentsWrapper(content::WebContents* web_contents)
      : tab_contents_(web_contents) {}
  virtual ~TabContentsWrapper() {}

  // AwContentsContainer
  virtual content::WebContents* GetWebContents() OVERRIDE {
    return tab_contents_.web_contents();
  }

 private:
  TabContents tab_contents_;
};

}  // namespace

AwBrowserDependencyFactoryImpl::AwBrowserDependencyFactoryImpl() {}

AwBrowserDependencyFactoryImpl::~AwBrowserDependencyFactoryImpl() {}

// static
void AwBrowserDependencyFactoryImpl::InstallInstance() {
  SetInstance(g_lazy_instance.Pointer());
}

content::WebContents*
AwBrowserDependencyFactoryImpl::CreateWebContents(bool incognito) {
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  if (incognito)
    profile = profile->GetOffTheRecordProfile();

  return content::WebContents::Create(profile, 0, MSG_ROUTING_NONE, 0);
}

AwContentsContainer* AwBrowserDependencyFactoryImpl::CreateContentsContainer(
    content::WebContents* contents) {
  return new TabContentsWrapper(contents);
}

content::JavaScriptDialogCreator*
    AwBrowserDependencyFactoryImpl::GetJavaScriptDialogCreator() {
  return GetJavaScriptDialogCreatorInstance();
}

}  // namespace android_webview

