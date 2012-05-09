// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_drag_dest_win.h"

#include <windows.h>
#include <shlobj.h>

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_drag_utils_win.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "ui/base/clipboard/clipboard_util_win.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/gfx/point.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/window_open_disposition.h"

using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;
using WebKit::WebDragOperationGeneric;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace {

// A helper method for getting the preferred drop effect.
DWORD GetPreferredDropEffect(DWORD effect) {
  if (effect & DROPEFFECT_COPY)
    return DROPEFFECT_COPY;
  if (effect & DROPEFFECT_LINK)
    return DROPEFFECT_LINK;
  if (effect & DROPEFFECT_MOVE)
    return DROPEFFECT_MOVE;
  return DROPEFFECT_NONE;
}

}  // namespace

// InterstitialDropTarget is like a ui::DropTarget implementation that
// WebDragDest passes through to if an interstitial is showing.  Rather than
// passing messages on to the renderer, we just check to see if there's a link
// in the drop data and handle links as navigations.
class InterstitialDropTarget {
 public:
  explicit InterstitialDropTarget(WebContents* web_contents)
      : web_contents_(web_contents) {}

  DWORD OnDragEnter(IDataObject* data_object, DWORD effect) {
    return ui::ClipboardUtil::HasUrl(data_object) ?
        GetPreferredDropEffect(effect) : DROPEFFECT_NONE;
  }

  DWORD OnDragOver(IDataObject* data_object, DWORD effect) {
    return ui::ClipboardUtil::HasUrl(data_object) ?
        GetPreferredDropEffect(effect) : DROPEFFECT_NONE;
  }

  void OnDragLeave(IDataObject* data_object) {
  }

  DWORD OnDrop(IDataObject* data_object, DWORD effect) {
    if (!ui::ClipboardUtil::HasUrl(data_object))
      return DROPEFFECT_NONE;

    std::wstring url;
    std::wstring title;
    ui::ClipboardUtil::GetUrl(data_object, &url, &title, true);
    OpenURLParams params(
        GURL(url), Referrer(), CURRENT_TAB,
        content::PAGE_TRANSITION_AUTO_BOOKMARK, false);
    web_contents_->OpenURL(params);
    return GetPreferredDropEffect(effect);
  }

 private:
  WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialDropTarget);
};

WebDragDest::WebDragDest(HWND source_hwnd, WebContents* web_contents)
    : ui::DropTarget(source_hwnd),
      web_contents_(web_contents),
      current_rvh_(NULL),
      drag_cursor_(WebDragOperationNone),
      interstitial_drop_target_(new InterstitialDropTarget(web_contents)),
      delegate_(NULL) {
}

WebDragDest::~WebDragDest() {
}

DWORD WebDragDest::OnDragEnter(IDataObject* data_object,
                               DWORD key_state,
                               POINT cursor_position,
                               DWORD effects) {
  current_rvh_ = web_contents_->GetRenderViewHost();

  if (delegate_)
    delegate_->DragInitialize(web_contents_);

  // Don't pass messages to the renderer if an interstitial page is showing
  // because we don't want the interstitial page to navigate.  Instead,
  // pass the messages on to a separate interstitial DropTarget handler.
  if (web_contents_->ShowingInterstitialPage())
    return interstitial_drop_target_->OnDragEnter(data_object, effects);

  // TODO(tc): PopulateWebDropData can be slow depending on what is in the
  // IDataObject.  Maybe we can do this in a background thread.
  drop_data_.reset(new WebDropData());
  WebDropData::PopulateWebDropData(data_object, drop_data_.get());

  if (drop_data_->url.is_empty())
    ui::OSExchangeDataProviderWin::GetPlainTextURL(data_object,
                                                   &drop_data_->url);

  drag_cursor_ = WebDragOperationNone;

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  web_contents_->GetRenderViewHost()->DragTargetDragEnter(*drop_data_,
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y),
      web_drag_utils_win::WinDragOpMaskToWebDragOpMask(effects));

  if (delegate_)
      delegate_->OnDragEnter(data_object);

  // We lie here and always return a DROPEFFECT because we don't want to
  // wait for the IPC call to return.
  return web_drag_utils_win::WebDragOpToWinDragOp(drag_cursor_);
}

DWORD WebDragDest::OnDragOver(IDataObject* data_object,
                              DWORD key_state,
                              POINT cursor_position,
                              DWORD effects) {
  DCHECK(current_rvh_);
  if (current_rvh_ != web_contents_->GetRenderViewHost())
    OnDragEnter(data_object, key_state, cursor_position, effects);

  if (web_contents_->ShowingInterstitialPage())
    return interstitial_drop_target_->OnDragOver(data_object, effects);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  web_contents_->GetRenderViewHost()->DragTargetDragOver(
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y),
      web_drag_utils_win::WinDragOpMaskToWebDragOpMask(effects));

  if (delegate_)
    delegate_->OnDragOver(data_object);

  return web_drag_utils_win::WebDragOpToWinDragOp(drag_cursor_);
}

void WebDragDest::OnDragLeave(IDataObject* data_object) {
  DCHECK(current_rvh_);
  if (current_rvh_ != web_contents_->GetRenderViewHost())
    return;

  if (web_contents_->ShowingInterstitialPage()) {
    interstitial_drop_target_->OnDragLeave(data_object);
  } else {
    web_contents_->GetRenderViewHost()->DragTargetDragLeave();
  }

  if (delegate_)
    delegate_->OnDragLeave(data_object);

  drop_data_.reset();
}

DWORD WebDragDest::OnDrop(IDataObject* data_object,
                          DWORD key_state,
                          POINT cursor_position,
                          DWORD effect) {
  DCHECK(current_rvh_);
  if (current_rvh_ != web_contents_->GetRenderViewHost())
    OnDragEnter(data_object, key_state, cursor_position, effect);

  if (web_contents_->ShowingInterstitialPage())
    interstitial_drop_target_->OnDragOver(data_object, effect);

  if (web_contents_->ShowingInterstitialPage())
    return interstitial_drop_target_->OnDrop(data_object, effect);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  web_contents_->GetRenderViewHost()->DragTargetDrop(
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y));

  if (delegate_)
    delegate_->OnDrop(data_object);

  current_rvh_ = NULL;

  // This isn't always correct, but at least it's a close approximation.
  // For now, we always map a move to a copy to prevent potential data loss.
  DWORD drop_effect = web_drag_utils_win::WebDragOpToWinDragOp(drag_cursor_);
  DWORD result = drop_effect != DROPEFFECT_MOVE ?
      drop_effect : DROPEFFECT_COPY;

  drop_data_.reset();
  return result;
}
