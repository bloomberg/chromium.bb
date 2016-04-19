// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/guest_information.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

using content::RenderProcessHost;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;
using extensions::Extension;
using guest_view::GuestViewBase;

namespace task_manager {

class GuestResource : public RendererResource {
 public:
  explicit GuestResource(content::RenderViewHost* render_view_host);
  ~GuestResource() override;

  // Resource methods:
  Type GetType() const override;
  base::string16 GetTitle() const override;
  gfx::ImageSkia GetIcon() const override;
  content::WebContents* GetWebContents() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestResource);
};

GuestResource::GuestResource(RenderViewHost* render_view_host)
    : RendererResource(
          render_view_host->GetSiteInstance()->GetProcess()->GetHandle(),
          render_view_host) {
}

GuestResource::~GuestResource() {
}

Resource::Type GuestResource::GetType() const {
  return GUEST;
}

base::string16 GuestResource::GetTitle() const {
  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    const int message_id = IDS_EXTENSION_TASK_MANAGER_WEBVIEW_TAG_PREFIX;
    return l10n_util::GetStringFUTF16(message_id, base::string16());
  }
  GuestViewBase* guest = GuestViewBase::FromWebContents(web_contents);
  return l10n_util::GetStringFUTF16(
      guest->GetTaskPrefix(),
      util::GetTitleFromWebContents(web_contents));
}

gfx::ImageSkia GuestResource::GetIcon() const {
  WebContents* web_contents = GetWebContents();
  if (web_contents) {
    favicon::FaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents);
    if (favicon_driver)
      return favicon_driver->GetFavicon().AsImageSkia();
  }
  return gfx::ImageSkia();
}

WebContents* GuestResource::GetWebContents() const {
  return WebContents::FromRenderViewHost(render_view_host());
}

////////////////////////////////////////////////////////////////////////////////
// GuestInformation class
////////////////////////////////////////////////////////////////////////////////

GuestInformation::GuestInformation() {}

GuestInformation::~GuestInformation() {}

bool GuestInformation::CheckOwnership(WebContents* web_contents) {
  // Guest WebContentses are created and owned internally by the content layer.
  return GuestViewBase::IsGuest(web_contents);
}

void GuestInformation::GetAll(const NewWebContentsCallback& callback) {
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderViewHost* rvh = content::RenderViewHost::From(widget);
    WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
    if (web_contents && GuestViewBase::IsGuest(web_contents))
      callback.Run(web_contents);
  }
}

std::unique_ptr<RendererResource> GuestInformation::MakeResource(
    WebContents* web_contents) {
  return std::unique_ptr<RendererResource>(
      new GuestResource(web_contents->GetRenderViewHost()));
}

}  // namespace task_manager
