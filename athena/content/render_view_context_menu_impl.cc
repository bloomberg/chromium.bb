// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/render_view_context_menu_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "components/renderer_context_menu/views/toolkit_delegate_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace athena {
namespace {

enum {
  CMD_BACK,
  CMD_FORWARD,
  CMD_RELOAD,
  CMD_VIEW_SOURCE,
  CMD_LAST,
};

// Max number of custom command ids allowd.
const int kNumCustomCommandIds = 1000;

void AppendPageItems(ui::SimpleMenuModel* menu_model) {
  menu_model->AddItem(CMD_BACK, base::ASCIIToUTF16("Back"));
  menu_model->AddItem(CMD_FORWARD, base::ASCIIToUTF16("Forward"));
  menu_model->AddItem(CMD_RELOAD, base::ASCIIToUTF16("Reload"));
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model->AddItem(CMD_VIEW_SOURCE, base::ASCIIToUTF16("View Source"));
}

}  // namespace

RenderViewContextMenuImpl::RenderViewContextMenuImpl(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : RenderViewContextMenuBase(render_frame_host, params) {
  SetContentCustomCommandIdRange(CMD_LAST, CMD_LAST + kNumCustomCommandIds);
  // TODO(oshima): Support other types
  set_content_type(
      new ContextMenuContentType(source_web_contents_, params, true));
  set_toolkit_delegate(scoped_ptr<ToolkitDelegate>(new ToolkitDelegateViews));
}

RenderViewContextMenuImpl::~RenderViewContextMenuImpl() {
}

void RenderViewContextMenuImpl::RunMenuAt(views::Widget* parent,
                                          const gfx::Point& point,
                                          ui::MenuSourceType type) {
  static_cast<ToolkitDelegateViews*>(toolkit_delegate())
      ->RunMenuAt(parent, point, type);
}

void RenderViewContextMenuImpl::InitMenu() {
  RenderViewContextMenuBase::InitMenu();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE)) {
    AppendPageItems(&menu_model_);
  }
}

void RenderViewContextMenuImpl::RecordShownItem(int id) {
  // TODO(oshima): Imelement UMA stats. crbug.com/401673
  NOTIMPLEMENTED();
}

void RenderViewContextMenuImpl::RecordUsedItem(int id) {
  // TODO(oshima): Imelement UMA stats. crbug.com/401673
  NOTIMPLEMENTED();
}

#if defined(ENABLE_PLUGINS)
void RenderViewContextMenuImpl::HandleAuthorizeAllPlugins() {
}
#endif

void RenderViewContextMenuImpl::NotifyMenuShown() {
}

void RenderViewContextMenuImpl::NotifyURLOpened(
    const GURL& url,
    content::WebContents* new_contents) {
}

bool RenderViewContextMenuImpl::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  NOTIMPLEMENTED();
  return false;
}

bool RenderViewContextMenuImpl::IsCommandIdChecked(int command_id) const {
  return false;
}

bool RenderViewContextMenuImpl::IsCommandIdEnabled(int command_id) const {
  if (RenderViewContextMenuBase::IsCommandIdEnabled(command_id))
    return true;
  switch (command_id) {
    case CMD_BACK:
      return source_web_contents_->GetController().CanGoBack();
    case CMD_FORWARD:
      return source_web_contents_->GetController().CanGoForward();
    case CMD_RELOAD:
      return true;
    case CMD_VIEW_SOURCE:
      return source_web_contents_->GetController().CanViewSource();
  }
  return false;
}

void RenderViewContextMenuImpl::ExecuteCommand(int command_id,
                                               int event_flags) {
  RenderViewContextMenuBase::ExecuteCommand(command_id, event_flags);
  if (command_executed_)
    return;
  command_executed_ = true;
  switch (command_id) {
    case CMD_BACK:
      source_web_contents_->GetController().GoBack();
      break;
    case CMD_FORWARD:
      source_web_contents_->GetController().GoForward();
      break;
    case CMD_RELOAD:
      source_web_contents_->GetController().Reload(true);
      break;
    case CMD_VIEW_SOURCE:
      source_web_contents_->ViewSource();
      break;
  }
}

}  // namespace athena
