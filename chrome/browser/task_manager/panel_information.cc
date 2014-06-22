// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/panel_information.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

using content::RenderProcessHost;
using content::RenderViewHost;
using content::WebContents;
using extensions::Extension;

namespace task_manager {

class PanelResource : public RendererResource {
 public:
  explicit PanelResource(Panel* panel);
  virtual ~PanelResource();

  // Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;

 private:
  Panel* panel_;
  // Determines prefix for title reflecting whether extensions are apps
  // or in incognito mode.
  int message_prefix_id_;

  DISALLOW_COPY_AND_ASSIGN(PanelResource);
};

PanelResource::PanelResource(Panel* panel)
    : RendererResource(
        panel->GetWebContents()->GetRenderProcessHost()->GetHandle(),
        panel->GetWebContents()->GetRenderViewHost()),
      panel_(panel) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(panel_->profile());
  message_prefix_id_ = util::GetMessagePrefixID(
      registry->enabled_extensions().GetByID(panel_->extension_id())->is_app(),
      true,  // is_extension
      panel_->profile()->IsOffTheRecord(),
      false,   // is_prerender
      false);  // is_background
}

PanelResource::~PanelResource() {
}

Resource::Type PanelResource::GetType() const {
  return EXTENSION;
}

base::string16 PanelResource::GetTitle() const {
  base::string16 title = panel_->GetWindowTitle();
  // Since the title will be concatenated with an IDS_TASK_MANAGER_* prefix
  // we need to explicitly set the title to be LTR format if there is no
  // strong RTL charater in it. Otherwise, if the task manager prefix is an
  // RTL word, the concatenated result might be wrong. For example,
  // a page whose title is "Yahoo! Mail: The best web-based Email!", without
  // setting it explicitly as LTR format, the concatenated result will be
  // "!Yahoo! Mail: The best web-based Email :PPA", in which the capital
  // letters "PPA" stands for the Hebrew word for "app".
  base::i18n::AdjustStringForLocaleDirection(&title);

  return l10n_util::GetStringFUTF16(message_prefix_id_, title);
}

gfx::ImageSkia PanelResource::GetIcon() const {
  gfx::Image icon = panel_->GetCurrentPageIcon();
  return icon.IsEmpty() ? gfx::ImageSkia() : *icon.ToImageSkia();
}

WebContents* PanelResource::GetWebContents() const {
  return panel_->GetWebContents();
}

////////////////////////////////////////////////////////////////////////////////
// PanelInformation class
////////////////////////////////////////////////////////////////////////////////

PanelInformation::PanelInformation() {}

PanelInformation::~PanelInformation() {}

bool PanelInformation::CheckOwnership(WebContents* web_contents) {
  std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
  for (size_t i = 0; i < panels.size(); ++i) {
    if (panels[i]->GetWebContents() == web_contents) {
      return true;
    }
  }
  return false;
}

void PanelInformation::GetAll(const NewWebContentsCallback& callback) {
  // Add all the Panels.
  std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
  for (size_t i = 0; i < panels.size(); ++i)
    callback.Run(panels[i]->GetWebContents());
}

scoped_ptr<RendererResource> PanelInformation::MakeResource(
    WebContents* web_contents) {
  std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
  for (size_t i = 0; i < panels.size(); ++i) {
    if (panels[i]->GetWebContents() == web_contents) {
      return scoped_ptr<RendererResource>(new PanelResource(panels[i]));
    }
  }
  NOTREACHED();
  return scoped_ptr<RendererResource>();
}

}  // namespace task_manager
