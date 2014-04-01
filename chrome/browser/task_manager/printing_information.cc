// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/printing_information.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::WebContents;

namespace task_manager {

class PrintingResource : public RendererResource {
 public:
  explicit PrintingResource(content::WebContents* web_contents);
  virtual ~PrintingResource();

  // Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;

 private:
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PrintingResource);
};

PrintingResource::PrintingResource(WebContents* web_contents)
    : RendererResource(web_contents->GetRenderProcessHost()->GetHandle(),
                       web_contents->GetRenderViewHost()),
      web_contents_(web_contents) {}

PrintingResource::~PrintingResource() {}

Resource::Type PrintingResource::GetType() const { return RENDERER; }

base::string16 PrintingResource::GetTitle() const {
  return l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_PRINT_PREFIX,
      util::GetTitleFromWebContents(web_contents_));
}

gfx::ImageSkia PrintingResource::GetIcon() const {
  FaviconTabHelper::CreateForWebContents(web_contents_);
  return FaviconTabHelper::FromWebContents(web_contents_)->
      GetFavicon().AsImageSkia();
}

WebContents* PrintingResource::GetWebContents() const { return web_contents_; }

////////////////////////////////////////////////////////////////////////////////
// PrintingInformation class
////////////////////////////////////////////////////////////////////////////////

PrintingInformation::PrintingInformation() {}

PrintingInformation::~PrintingInformation() {}

bool PrintingInformation::CheckOwnership(WebContents* web_contents) {
  printing::BackgroundPrintingManager* backgrounded_print_jobs =
      g_browser_process->background_printing_manager();
  // Is it a background print job?
  if (backgrounded_print_jobs->HasPrintPreviewDialog(web_contents)) {
    return true;
  }
  // Is it a foreground print preview window?
  if (printing::PrintPreviewDialogController::IsPrintPreviewDialog(
          web_contents)) {
    return true;
  }
  return false;
}

void PrintingInformation::GetAll(const NewWebContentsCallback& callback) {
  // Add all the pages being background printed.
  printing::BackgroundPrintingManager* printing_manager =
      g_browser_process->background_printing_manager();
  std::set<content::WebContents*> printing_contents =
      printing_manager->CurrentContentSet();
  for (std::set<content::WebContents*>::iterator i = printing_contents.begin();
       i != printing_contents.end();
       ++i) {
    callback.Run(*i);
  }
  // Add all the foreground print preview dialogs.
  printing::PrintPreviewDialogController::GetInstance()->ForEachPreviewDialog(
      callback);
}

scoped_ptr<RendererResource> PrintingInformation::MakeResource(
    WebContents* web_contents) {
  return scoped_ptr<RendererResource>(new PrintingResource(web_contents));
}

}  // namespace task_manager
