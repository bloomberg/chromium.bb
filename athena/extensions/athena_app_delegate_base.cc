// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/athena_app_delegate_base.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/env/public/athena_env.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/common/constants.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"

namespace athena {
namespace {

content::WebContents* OpenURLInActivity(content::BrowserContext* context,
                                        const content::OpenURLParams& params) {
  // Force all links to open in a new activity.
  Activity* activity = ActivityFactory::Get()->CreateWebActivity(
      context, base::string16(), params.url);
  Activity::Show(activity);
  // TODO(oshima): Get the web cotnents from activity.
  return NULL;
}

}  // namespace

// This is a extra step to open a new Activity when a link is simply clicked
// on an app activity (which usually replaces the content).
class AthenaAppDelegateBase::NewActivityContentsDelegate
    : public content::WebContentsDelegate {
 public:
  NewActivityContentsDelegate() {}
  virtual ~NewActivityContentsDelegate() {}

  // content::WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE {
    if (!source)
      return NULL;
    return OpenURLInActivity(source->GetBrowserContext(), params);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NewActivityContentsDelegate);
};

AthenaAppDelegateBase::AthenaAppDelegateBase()
    : new_window_contents_delegate_(new NewActivityContentsDelegate) {
}

AthenaAppDelegateBase::~AthenaAppDelegateBase() {
  if (!terminating_callback_.is_null())
    AthenaEnv::Get()->RemoveTerminatingCallback(terminating_callback_);
}

void AthenaAppDelegateBase::ResizeWebContents(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  aura::Window* window = web_contents->GetNativeView();
  window->SetBounds(gfx::Rect(window->bounds().origin(), size));
}

content::WebContents* AthenaAppDelegateBase::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return OpenURLInActivity(context, params);
}

void AthenaAppDelegateBase::AddNewContents(content::BrowserContext* context,
                                           content::WebContents* new_contents,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_pos,
                                           bool user_gesture,
                                           bool* was_blocked) {
  new_contents->SetDelegate(new_window_contents_delegate_.get());
}

int AthenaAppDelegateBase::PreferredIconSize() {
  // TODO(oshima): Find out what to use.
  return extension_misc::EXTENSION_ICON_SMALL;
}

bool AthenaAppDelegateBase::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return web_contents->GetNativeView()->IsVisible();
}

void AthenaAppDelegateBase::SetTerminatingCallback(
    const base::Closure& callback) {
  if (!terminating_callback_.is_null())
    AthenaEnv::Get()->RemoveTerminatingCallback(terminating_callback_);
  terminating_callback_ = callback;
  if (!terminating_callback_.is_null())
    AthenaEnv::Get()->AddTerminatingCallback(terminating_callback_);
}

}  // namespace athena
