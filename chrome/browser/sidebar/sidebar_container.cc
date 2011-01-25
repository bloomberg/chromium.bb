// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sidebar/sidebar_container.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/extension_sidebar_defaults.h"
#include "chrome/common/extensions/extension_sidebar_utils.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

SidebarContainer::SidebarContainer(TabContents* tab,
                                   const std::string& content_id,
                                   Delegate* delegate)
    : tab_(tab),
      content_id_(content_id),
      delegate_(delegate),
      icon_(new SkBitmap),
      navigate_to_default_page_on_expand_(true),
      use_default_icon_(true) {
  // Create TabContents for sidebar.
  sidebar_contents_.reset(
      new TabContents(tab->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL));
  sidebar_contents_->render_view_host()->set_is_extension_process(true);
  sidebar_contents_->render_view_host()->AllowBindings(
      BindingsPolicy::EXTENSION);
  sidebar_contents_->set_delegate(this);
}

SidebarContainer::~SidebarContainer() {
}

void SidebarContainer::SidebarClosing() {
  delegate_->UpdateSidebar(this);
}

void SidebarContainer::LoadDefaults() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;  // Can be NULL in tests.
  const ExtensionSidebarDefaults* sidebar_defaults =
      extension->sidebar_defaults();

  title_ = sidebar_defaults->default_title();

  if (!sidebar_defaults->default_icon_path().empty()) {
    image_loading_tracker_.reset(new ImageLoadingTracker(this));
    image_loading_tracker_->LoadImage(
        extension,
        extension->GetResource(sidebar_defaults->default_icon_path()),
        gfx::Size(Extension::kSidebarIconMaxSize,
                  Extension::kSidebarIconMaxSize),
        ImageLoadingTracker::CACHE);
  }
}

void SidebarContainer::Show() {
  delegate_->UpdateSidebar(this);
}

void SidebarContainer::Expand() {
  if (navigate_to_default_page_on_expand_) {
    navigate_to_default_page_on_expand_ = false;
    // Check whether a default page is specified for this sidebar.
    const Extension* extension = GetExtension();
    if (extension) {  // Can be NULL in tests.
      if (extension->sidebar_defaults()->default_page().is_valid())
        Navigate(extension->sidebar_defaults()->default_page());
    }
  }

  delegate_->UpdateSidebar(this);
  sidebar_contents_->view()->SetInitialFocus();
}

void SidebarContainer::Collapse() {
  delegate_->UpdateSidebar(this);
}

void SidebarContainer::Navigate(const GURL& url) {
  // TODO(alekseys): add a progress UI.
  navigate_to_default_page_on_expand_ = false;
  sidebar_contents_->controller().LoadURL(
      url, GURL(), PageTransition::START_PAGE);
}

void SidebarContainer::SetBadgeText(const string16& badge_text) {
  badge_text_ = badge_text;
}

void SidebarContainer::SetIcon(const SkBitmap& bitmap) {
  use_default_icon_ = false;
  *icon_ = bitmap;
}

void SidebarContainer::SetTitle(const string16& title) {
  title_ = title;
}

bool SidebarContainer::IsPopup(const TabContents* source) const {
  return false;
}

void SidebarContainer::OnImageLoaded(SkBitmap* image,
                                     ExtensionResource resource,
                                     int index) {
  if (image && use_default_icon_) {
    *icon_ = *image;
     delegate_->UpdateSidebar(this);
  }
}

const Extension* SidebarContainer::GetExtension() const {
  ExtensionService* service =
      sidebar_contents_->profile()->GetExtensionService();
  if (!service)
    return NULL;
  return service->GetExtensionById(
      extension_sidebar_utils::GetExtensionIdByContentId(content_id_), false);
}
