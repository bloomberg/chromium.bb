// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/guest_view/web_view/chrome_web_view_guest_delegate.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/chrome_version_info.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

void RemoveWebViewEventListenersOnIOThread(
    void* profile,
    const std::string& extension_id,
    int embedder_process_id,
    int view_instance_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  ExtensionWebRequestEventRouter::GetInstance()->RemoveWebViewEventListeners(
      profile,
      extension_id,
      embedder_process_id,
      view_instance_id);
}

ChromeWebViewGuestDelegate::ChromeWebViewGuestDelegate(
    extensions::WebViewGuest* web_view_guest)
    : pending_context_menu_request_id_(0),
      chromevox_injected_(false),
      current_zoom_factor_(1.0),
      web_view_guest_(web_view_guest) {
}

ChromeWebViewGuestDelegate::~ChromeWebViewGuestDelegate() {
}

double ChromeWebViewGuestDelegate::GetZoom() {
  return current_zoom_factor_;
}

bool ChromeWebViewGuestDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  DCHECK(menu_delegate);

  pending_menu_ = menu_delegate->BuildMenu(guest_web_contents(), params);

  // Pass it to embedder.
  int request_id = ++pending_context_menu_request_id_;
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  scoped_ptr<base::ListValue> items =
      MenuModelToValue(pending_menu_->menu_model());
  args->Set(webview::kContextMenuItems, items.release());
  args->SetInteger(webview::kRequestId, request_id);
  web_view_guest()->DispatchEventToEmbedder(
      new extensions::GuestViewBase::Event(
          webview::kEventContextMenu, args.Pass()));
  return true;
}

// TODO(hanxi) Investigate which of these observers should move to the
// extension module in the future.
void ChromeWebViewGuestDelegate::OnAttachWebViewHelpers(
    content::WebContents* contents) {
  // Create a zoom controller for the guest contents give it access to
  // GetZoomLevel() and and SetZoomLevel() in WebViewGuest.
  // TODO(wjmaclean) This currently uses the same HostZoomMap as the browser
  // context, but we eventually want to isolate the guest contents from zoom
  // changes outside the guest (e.g. in the main browser), so we should
  // create a separate HostZoomMap for the guest.
  ZoomController::CreateForWebContents(contents);

  FaviconTabHelper::CreateForWebContents(contents);
  extensions::ChromeExtensionWebContentsObserver::
      CreateForWebContents(contents);
#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
  printing::PrintViewManager::CreateForWebContents(contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(contents);
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      contents,
      scoped_ptr<pdf::PDFWebContentsHelperClient>(
          new ChromePDFWebContentsHelperClient()));
}

void ChromeWebViewGuestDelegate::OnEmbedderDestroyed() {
  // TODO(fsamuel): WebRequest event listeners for <webview> should survive
  // reparenting of a <webview> within a single embedder. Right now, we keep
  // around the browser state for the listener for the lifetime of the embedder.
  // Ideally, the lifetime of the listeners should match the lifetime of the
  // <webview> DOM node. Once http://crbug.com/156219 is resolved we can move
  // the call to RemoveWebViewEventListenersOnIOThread back to
  // WebViewGuest::WebContentsDestroyed.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &RemoveWebViewEventListenersOnIOThread,
          web_view_guest()->browser_context(),
          web_view_guest()->embedder_extension_id(),
          web_view_guest()->embedder_render_process_id(),
          web_view_guest()->view_instance_id()));
}

void ChromeWebViewGuestDelegate::OnDidAttachToEmbedder() {
  // TODO(fsamuel): This code should be implemented in GuestViewBase once the
  // ZoomController moves to the extensions module.
  ZoomController* zoom_controller = ZoomController::FromWebContents(
      web_view_guest()->embedder_web_contents());
  if (!zoom_controller)
    return;
  // Listen to the embedder's zoom changes.
  zoom_controller->AddObserver(this);
  // Set the guest's initial zoom level to be equal to the embedder's.
  ZoomController::FromWebContents(guest_web_contents())->
      SetZoomLevel(zoom_controller->GetZoomLevel());
}

void ChromeWebViewGuestDelegate::OnDidCommitProvisionalLoadForFrame(
    bool is_main_frame) {
  // Update the current zoom factor for the new page.
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(guest_web_contents());
  DCHECK(zoom_controller);
  current_zoom_factor_ = zoom_controller->GetZoomLevel();
  if (is_main_frame)
    chromevox_injected_ = false;
}

void ChromeWebViewGuestDelegate::OnDidInitialize() {
#if defined(OS_CHROMEOS)
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::Bind(&ChromeWebViewGuestDelegate::OnAccessibilityStatusChanged,
                 base::Unretained(this)));
#endif
}

void ChromeWebViewGuestDelegate::OnDocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent())
    InjectChromeVoxIfNeeded(render_frame_host->GetRenderViewHost());
}

void ChromeWebViewGuestDelegate::OnGuestDestroyed() {
  // Clean up custom context menu items for this guest.
  extensions::MenuManager* menu_manager = extensions::MenuManager::Get(
      Profile::FromBrowserContext(web_view_guest()->browser_context()));
  menu_manager->RemoveAllContextItems(extensions::MenuItem::ExtensionKey(
      web_view_guest()->embedder_extension_id(),
      web_view_guest()->view_instance_id()));
}

// static
scoped_ptr<base::ListValue> ChromeWebViewGuestDelegate::MenuModelToValue(
    const ui::SimpleMenuModel& menu_model) {
  scoped_ptr<base::ListValue> items(new base::ListValue());
  for (int i = 0; i < menu_model.GetItemCount(); ++i) {
    base::DictionaryValue* item_value = new base::DictionaryValue();
    // TODO(lazyboy): We need to expose some kind of enum equivalent of
    // |command_id| instead of plain integers.
    item_value->SetInteger(webview::kMenuItemCommandId,
                           menu_model.GetCommandIdAt(i));
    item_value->SetString(webview::kMenuItemLabel, menu_model.GetLabelAt(i));
    items->Append(item_value);
  }
  return items.Pass();
}

void ChromeWebViewGuestDelegate::OnSetZoom(double zoom_factor) {
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(guest_web_contents());
  DCHECK(zoom_controller);
  double zoom_level = content::ZoomFactorToZoomLevel(zoom_factor);
  zoom_controller->SetZoomLevel(zoom_level);

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetDouble(webview::kOldZoomFactor, current_zoom_factor_);
  args->SetDouble(webview::kNewZoomFactor, zoom_factor);
  web_view_guest()->DispatchEventToEmbedder(
      new extensions::GuestViewBase::Event(
          webview::kEventZoomChange, args.Pass()));
  current_zoom_factor_ = zoom_factor;
}

void ChromeWebViewGuestDelegate::OnShowContextMenu(
    int request_id,
    const MenuItemVector* items) {
  if (!pending_menu_.get())
    return;

  // Make sure this was the correct request.
  if (request_id != pending_context_menu_request_id_)
    return;

  // TODO(lazyboy): Implement.
  DCHECK(!items);

  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  menu_delegate->ShowMenu(pending_menu_.Pass());
}

void ChromeWebViewGuestDelegate::InjectChromeVoxIfNeeded(
    content::RenderViewHost* render_view_host) {
#if defined(OS_CHROMEOS)
  if (!chromevox_injected_) {
    chromeos::AccessibilityManager* manager =
        chromeos::AccessibilityManager::Get();
    if (manager && manager->IsSpokenFeedbackEnabled()) {
      manager->InjectChromeVox(render_view_host);
      chromevox_injected_ = true;
    }
  }
#endif
}

#if defined(OS_CHROMEOS)
void ChromeWebViewGuestDelegate::OnAccessibilityStatusChanged(
    const chromeos::AccessibilityStatusEventDetails& details) {
  if (details.notification_type == chromeos::ACCESSIBILITY_MANAGER_SHUTDOWN) {
    accessibility_subscription_.reset();
  } else if (details.notification_type ==
      chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK) {
    if (details.enabled)
      InjectChromeVoxIfNeeded(guest_web_contents()->GetRenderViewHost());
    else
      chromevox_injected_ = false;
  }
}
#endif

void ChromeWebViewGuestDelegate::OnZoomChanged(
    const ZoomController::ZoomChangedEventData& data) {
  ZoomController::FromWebContents(guest_web_contents())->
      SetZoomLevel(data.new_zoom_level);
}
