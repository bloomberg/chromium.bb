// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/content_settings.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/page_type.h"
#include "content/public/common/security_style.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/upload_data.h"
#include "net/cert/cert_status_flags.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

IPC_ENUM_TRAITS(AutomationMsg_NavigationResponseValues)
IPC_ENUM_TRAITS(content::PageType)

IPC_STRUCT_BEGIN(AutomationMsg_Find_Params)
  // The word(s) to find on the page.
  IPC_STRUCT_MEMBER(base::string16, search_string)

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
  IPC_STRUCT_MEMBER(uint64, upload_size)
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

IPC_STRUCT_BEGIN(AutomationURLRequest)
  IPC_STRUCT_MEMBER(std::string, url)
  IPC_STRUCT_MEMBER(std::string, method)
  IPC_STRUCT_MEMBER(std::string, referrer)
  IPC_STRUCT_MEMBER(std::string, extra_request_headers)
  IPC_STRUCT_MEMBER(scoped_refptr<net::UploadData>, upload_data)
  IPC_STRUCT_MEMBER(int, resource_type)  // see webkit/common/resource_type.h
  IPC_STRUCT_MEMBER(int, load_flags) // see net/base/load_flags.h
IPC_STRUCT_END()

// Singly-included section for struct and custom IPC traits.
#ifndef CHROME_COMMON_AUTOMATION_MESSAGES_H_
#define CHROME_COMMON_AUTOMATION_MESSAGES_H_

namespace IPC {

template <>
struct ParamTraits<scoped_refptr<net::UploadData> > {
  typedef scoped_refptr<net::UploadData> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<net::URLRequestStatus> {
  typedef net::URLRequestStatus param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_AUTOMATION_MESSAGES_H_

// Keep this internal message file unchanged to preserve line numbering
// (and hence the dubious __LINE__-based message numberings) across versions.
#include "chrome/common/automation_messages_internal.h"
