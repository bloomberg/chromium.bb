// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/content_settings.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/page_type.h"
#include "content/public/common/security_style.h"
#include "content/public/common/webkit_param_traits.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/cert_status_flags.h"
#include "net/base/host_port_pair.h"
#include "net/base/upload_data.h"
#include "net/url_request/url_request_status.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/window_open_disposition.h"

IPC_ENUM_TRAITS(AutomationMsg_NavigationResponseValues)
IPC_ENUM_TRAITS(content::PageType)

IPC_STRUCT_BEGIN(AutomationMsg_Find_Params)
  // The word(s) to find on the page.
  IPC_STRUCT_MEMBER(string16, search_string)

  // Whether to search forward or backward within the page.
  IPC_STRUCT_MEMBER(bool, forward)

  // Whether search should be Case sensitive.
  IPC_STRUCT_MEMBER(bool, match_case)

  // Whether this operation is first request (Find) or a follow-up (FindNext).
  IPC_STRUCT_MEMBER(bool, find_next)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AutomationURLResponse)
  IPC_STRUCT_MEMBER(std::string, mime_type)
  IPC_STRUCT_MEMBER(std::string, headers)
  IPC_STRUCT_MEMBER(int64, content_length)
  IPC_STRUCT_MEMBER(base::Time, last_modified)
  IPC_STRUCT_MEMBER(std::string, redirect_url)
  IPC_STRUCT_MEMBER(int, redirect_status)
  IPC_STRUCT_MEMBER(net::HostPortPair, socket_address)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ExternalTabSettings)
  IPC_STRUCT_MEMBER(gfx::NativeWindow, parent)
  IPC_STRUCT_MEMBER(gfx::Rect, dimensions)
  IPC_STRUCT_MEMBER(unsigned int, style)
  IPC_STRUCT_MEMBER(bool, is_incognito)
  IPC_STRUCT_MEMBER(bool, load_requests_via_automation)
  IPC_STRUCT_MEMBER(bool, handle_top_level_requests)
  IPC_STRUCT_MEMBER(GURL, initial_url)
  IPC_STRUCT_MEMBER(GURL, referrer)
  IPC_STRUCT_MEMBER(bool, infobars_enabled)
  IPC_STRUCT_MEMBER(bool, route_all_top_level_navigations)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(NavigationInfo)
  IPC_STRUCT_MEMBER(int, navigation_type)
  IPC_STRUCT_MEMBER(int, relative_offset)
  IPC_STRUCT_MEMBER(int, navigation_index)
  IPC_STRUCT_MEMBER(std::wstring, title)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(GURL, referrer)
  IPC_STRUCT_MEMBER(content::SecurityStyle, security_style)
  IPC_STRUCT_MEMBER(bool, displayed_insecure_content)
  IPC_STRUCT_MEMBER(bool, ran_insecure_content)
IPC_STRUCT_END()

// A stripped down version of ContextMenuParams.
IPC_STRUCT_BEGIN(MiniContextMenuParams)
  // The x coordinate for displaying the menu.
  IPC_STRUCT_MEMBER(int, screen_x)

  // The y coordinate for displaying the menu.
  IPC_STRUCT_MEMBER(int, screen_y)

  // This is the URL of the link that encloses the node the context menu was
  // invoked on.
  IPC_STRUCT_MEMBER(GURL, link_url)

  // The link URL to be used ONLY for "copy link address". We don't validate
  // this field in the frontend process.
  IPC_STRUCT_MEMBER(GURL, unfiltered_link_url)

  // This is the source URL for the element that the context menu was
  // invoked on.  Example of elements with source URLs are img, audio, and
  // video.
  IPC_STRUCT_MEMBER(GURL, src_url)

  // This is the URL of the top level page that the context menu was invoked
  // on.
  IPC_STRUCT_MEMBER(GURL, page_url)

  // This is the absolute keyword search URL including the %s search tag when
  // the "Add as search engine..." option is clicked (left empty if not used).
  IPC_STRUCT_MEMBER(GURL, keyword_url)

  // This is the URL of the subframe that the context menu was invoked on.
  IPC_STRUCT_MEMBER(GURL, frame_url)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AttachExternalTabParams)
  IPC_STRUCT_MEMBER(uint64, cookie)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(gfx::Rect, dimensions)
  IPC_STRUCT_MEMBER(int, disposition)
  IPC_STRUCT_MEMBER(bool, user_gesture)
  IPC_STRUCT_MEMBER(std::string, profile_name)
IPC_STRUCT_END()

#if defined(OS_WIN) && !defined(USE_AURA)

IPC_STRUCT_BEGIN(Reposition_Params)
  IPC_STRUCT_MEMBER(HWND, window)
  IPC_STRUCT_MEMBER(HWND, window_insert_after)
  IPC_STRUCT_MEMBER(int, left)
  IPC_STRUCT_MEMBER(int, top)
  IPC_STRUCT_MEMBER(int, width)
  IPC_STRUCT_MEMBER(int, height)
  IPC_STRUCT_MEMBER(int, flags)
  IPC_STRUCT_MEMBER(bool, set_parent)
  IPC_STRUCT_MEMBER(HWND, parent_window)
IPC_STRUCT_END()

#endif  // defined(OS_WIN)

IPC_STRUCT_BEGIN(AutomationURLRequest)
  IPC_STRUCT_MEMBER(std::string, url)
  IPC_STRUCT_MEMBER(std::string, method)
  IPC_STRUCT_MEMBER(std::string, referrer)
  IPC_STRUCT_MEMBER(std::string, extra_request_headers)
  IPC_STRUCT_MEMBER(scoped_refptr<net::UploadData>, upload_data)
  IPC_STRUCT_MEMBER(int, resource_type)  // see webkit/glue/resource_type.h
  IPC_STRUCT_MEMBER(int, load_flags) // see net/base/load_flags.h
IPC_STRUCT_END()

// Singly-included section for struct and custom IPC traits.
#ifndef CHROME_COMMON_AUTOMATION_MESSAGES_H_
#define CHROME_COMMON_AUTOMATION_MESSAGES_H_

// This struct passes information about the context menu in Chrome stored
// as a ui::MenuModel to Chrome Frame.  It is basically a container of
// items that go in the menu.  An item may be a submenu, so the data
// structure may be a tree.
struct ContextMenuModel {
  ContextMenuModel();
  ~ContextMenuModel();

  // This struct describes one item in the menu.
  struct Item {
    Item();

    // This is the type of the menu item, using values from the enum
    // ui::MenuModel::ItemType (serialized as an int).
    int type;

    // This is the command id of the menu item, which will be passed by
    // Chrome Frame to Chrome if the item is selected.
    int item_id;

    // This the the menu label, if needed.
    std::wstring label;

    // These are states of the menu item.
    bool checked;
    bool enabled;

    // This recursively describes the submenu if type is
    // ui::MenuModel::TYPE_SUBMENU.
    ContextMenuModel* submenu;
  };

  // This is the list of menu items.
  std::vector<Item> items;
};

namespace IPC {

// Traits for ContextMenuModel structure to pack/unpack.
template <>
struct ParamTraits<ContextMenuModel> {
  typedef ContextMenuModel param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_AUTOMATION_MESSAGES_H_

// Keep this internal message file unchanged to preserve line numbering
// (and hence the dubious __LINE__-based message numberings) across versions.
#include "chrome/common/automation_messages_internal.h"
