// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drop_target_win.h"

#include <windows.h>
#include <shlobj.h>

#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/web_drag_utils_win.h"
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

// InterstitialDropTarget is like a app::win::DropTarget implementation that
// WebDropTarget passes through to if an interstitial is showing.  Rather than
// passing messages on to the renderer, we just check to see if there's a link
// in the drop data and handle links as navigations.
class InterstitialDropTarget {
 public:
  explicit InterstitialDropTarget(TabContents* tab_contents)
      : tab_contents_(tab_contents) {}

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
    if (ui::ClipboardUtil::HasUrl(data_object)) {
      std::wstring url;
      std::wstring title;
      ui::ClipboardUtil::GetUrl(data_object, &url, &title, true);
      tab_contents_->OpenURL(GURL(url), GURL(), CURRENT_TAB,
                             PageTransition::AUTO_BOOKMARK);
      return GetPreferredDropEffect(effect);
    }
    return DROPEFFECT_NONE;
  }

 private:
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialDropTarget);
};

WebDropTarget::WebDropTarget(HWND source_hwnd, TabContents* tab_contents)
    : ui::DropTarget(source_hwnd),
      tab_contents_(tab_contents),
      current_rvh_(NULL),
      drag_cursor_(WebDragOperationNone),
      interstitial_drop_target_(new InterstitialDropTarget(tab_contents)) {
}

WebDropTarget::~WebDropTarget() {
}

DWORD WebDropTarget::OnDragEnter(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effects) {
  current_rvh_ = tab_contents_->render_view_host();

  // Don't pass messages to the renderer if an interstitial page is showing
  // because we don't want the interstitial page to navigate.  Instead,
  // pass the messages on to a separate interstitial DropTarget handler.
  if (tab_contents_->showing_interstitial_page())
    return interstitial_drop_target_->OnDragEnter(data_object, effects);

  // TODO(tc): PopulateWebDropData can be slow depending on what is in the
  // IDataObject.  Maybe we can do this in a background thread.
  WebDropData drop_data(GetDragIdentity());
  WebDropData::PopulateWebDropData(data_object, &drop_data);

  if (drop_data.url.is_empty())
    ui::OSExchangeDataProviderWin::GetPlainTextURL(data_object, &drop_data.url);

  drag_cursor_ = WebDragOperationNone;

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  tab_contents_->render_view_host()->DragTargetDragEnter(drop_data,
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y),
      web_drag_utils_win::WinDragOpMaskToWebDragOpMask(effects));

  // This is non-null if tab_contents_ is showing an ExtensionWebUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (tab_contents_->GetBookmarkDragDelegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      tab_contents_->GetBookmarkDragDelegate()->OnDragEnter(bookmark_drag_data);
  }

  // We lie here and always return a DROPEFFECT because we don't want to
  // wait for the IPC call to return.
  return web_drag_utils_win::WebDragOpToWinDragOp(drag_cursor_);
}

DWORD WebDropTarget::OnDragOver(IDataObject* data_object,
                                DWORD key_state,
                                POINT cursor_position,
                                DWORD effects) {
  DCHECK(current_rvh_);
  if (current_rvh_ != tab_contents_->render_view_host())
    OnDragEnter(data_object, key_state, cursor_position, effects);

  if (tab_contents_->showing_interstitial_page())
    return interstitial_drop_target_->OnDragOver(data_object, effects);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  tab_contents_->render_view_host()->DragTargetDragOver(
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y),
      web_drag_utils_win::WinDragOpMaskToWebDragOpMask(effects));

  if (tab_contents_->GetBookmarkDragDelegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      tab_contents_->GetBookmarkDragDelegate()->OnDragOver(bookmark_drag_data);
  }

  return web_drag_utils_win::WebDragOpToWinDragOp(drag_cursor_);
}

void WebDropTarget::OnDragLeave(IDataObject* data_object) {
  DCHECK(current_rvh_);
  if (current_rvh_ != tab_contents_->render_view_host())
    return;

  if (tab_contents_->showing_interstitial_page()) {
    interstitial_drop_target_->OnDragLeave(data_object);
  } else {
    tab_contents_->render_view_host()->DragTargetDragLeave();
  }

  if (tab_contents_->GetBookmarkDragDelegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      tab_contents_->GetBookmarkDragDelegate()->OnDragLeave(bookmark_drag_data);
  }
}

DWORD WebDropTarget::OnDrop(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect) {
  DCHECK(current_rvh_);
  if (current_rvh_ != tab_contents_->render_view_host())
    OnDragEnter(data_object, key_state, cursor_position, effect);

  if (tab_contents_->showing_interstitial_page())
    interstitial_drop_target_->OnDragOver(data_object, effect);

  if (tab_contents_->showing_interstitial_page())
    return interstitial_drop_target_->OnDrop(data_object, effect);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  tab_contents_->render_view_host()->DragTargetDrop(
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y));

  if (tab_contents_->GetBookmarkDragDelegate()) {
    ui::OSExchangeData os_exchange_data(
        new ui::OSExchangeDataProviderWin(data_object));
    BookmarkNodeData bookmark_drag_data;
    if (bookmark_drag_data.Read(os_exchange_data))
      tab_contents_->GetBookmarkDragDelegate()->OnDrop(bookmark_drag_data);
  }

  current_rvh_ = NULL;

  // This isn't always correct, but at least it's a close approximation.
  // For now, we always map a move to a copy to prevent potential data loss.
  DWORD drop_effect = web_drag_utils_win::WebDragOpToWinDragOp(drag_cursor_);
  return drop_effect != DROPEFFECT_MOVE ? drop_effect : DROPEFFECT_COPY;
}
