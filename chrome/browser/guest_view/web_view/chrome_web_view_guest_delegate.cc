// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/guest_view/web_view/chrome_web_view_guest_delegate.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "chrome/common/url_constants.h"
#include "components/browsing_data/content/storage_partition_http_cache_data_remover.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"

using guest_view::GuestViewEvent;

namespace extensions {

ChromeWebViewGuestDelegate::ChromeWebViewGuestDelegate(
    WebViewGuest* web_view_guest)
    : pending_context_menu_request_id_(0),
      web_view_guest_(web_view_guest),
      weak_ptr_factory_(this) {
}

ChromeWebViewGuestDelegate::~ChromeWebViewGuestDelegate() {
}

bool ChromeWebViewGuestDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  DCHECK(menu_delegate);

  content::ContextMenuParams new_params = params;
  // The only case where |context_menu_position_| is not initialized is the case
  // where the input event is directly sent to the guest WebContents without
  // ever going throught the embedder and BrowserPlugin's
  // RenderWidgetHostViewGuest. This only happens in some tests, e.g.,
  // WebViewInteractiveTest.ContextMenuParamCoordinates.
  if (context_menu_position_) {
    new_params.x = context_menu_position_->x();
    new_params.y = context_menu_position_->y();
  }

  pending_menu_ = menu_delegate->BuildMenu(guest_web_contents(), new_params);
  // It's possible for the returned menu to be null, so early out to avoid
  // a crash. TODO(wjmaclean): find out why it's possible for this to happen
  // in the first place, and if it's an error.
  if (!pending_menu_)
    return false;

  // Pass it to embedder.
  int request_id = ++pending_context_menu_request_id_;
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  std::unique_ptr<base::ListValue> items =
      MenuModelToValue(pending_menu_->menu_model());
  args->Set(webview::kContextMenuItems, items.release());
  args->SetInteger(webview::kRequestId, request_id);
  web_view_guest()->DispatchEventToView(base::MakeUnique<GuestViewEvent>(
      webview::kEventContextMenuShow, std::move(args)));
  return true;
}

// static
std::unique_ptr<base::ListValue> ChromeWebViewGuestDelegate::MenuModelToValue(
    const ui::SimpleMenuModel& menu_model) {
  std::unique_ptr<base::ListValue> items(new base::ListValue());
  for (int i = 0; i < menu_model.GetItemCount(); ++i) {
    std::unique_ptr<base::DictionaryValue> item_value(
        new base::DictionaryValue());
    // TODO(lazyboy): We need to expose some kind of enum equivalent of
    // |command_id| instead of plain integers.
    item_value->SetInteger(webview::kMenuItemCommandId,
                           menu_model.GetCommandIdAt(i));
    item_value->SetString(webview::kMenuItemLabel, menu_model.GetLabelAt(i));
    items->Append(std::move(item_value));
  }
  return items;
}

void ChromeWebViewGuestDelegate::OnShowContextMenu(int request_id) {
  if (!pending_menu_)
    return;

  // Make sure this was the correct request.
  if (request_id != pending_context_menu_request_id_)
    return;

  // TODO(lazyboy): Implement.

  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  menu_delegate->ShowMenu(std::move(pending_menu_));
}

bool ChromeWebViewGuestDelegate::ShouldHandleFindRequestsForEmbedder() const {
  // Find requests will be handled by the guest for the Chrome signin page.
  return web_view_guest_->owner_web_contents()->GetWebUI() != nullptr &&
         web_view_guest_->GetOwnerSiteURL().GetOrigin().spec() ==
             chrome::kChromeUIChromeSigninURL;
}

void ChromeWebViewGuestDelegate::SetContextMenuPosition(
    const gfx::Point& position) {
  if (context_menu_position_ == nullptr)
    context_menu_position_.reset(new gfx::Point());

  *context_menu_position_ = position;
}

}  // namespace extensions
