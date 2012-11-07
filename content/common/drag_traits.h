// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/drag_event_source_info.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/gfx/point.h"
#include "webkit/glue/webdropdata.h"

#define IPC_MESSAGE_START DragMsgStart

IPC_ENUM_TRAITS(WebKit::WebDragOperation)

IPC_STRUCT_TRAITS_BEGIN(WebDropData::FileInfo)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebDropData)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(url_title)
  IPC_STRUCT_TRAITS_MEMBER(download_metadata)
  IPC_STRUCT_TRAITS_MEMBER(referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(filenames)
  IPC_STRUCT_TRAITS_MEMBER(filesystem_id)
  IPC_STRUCT_TRAITS_MEMBER(text)
  IPC_STRUCT_TRAITS_MEMBER(html)
  IPC_STRUCT_TRAITS_MEMBER(html_base_url)
  IPC_STRUCT_TRAITS_MEMBER(file_description_filename)
  IPC_STRUCT_TRAITS_MEMBER(file_contents)
  IPC_STRUCT_TRAITS_MEMBER(custom_data)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(ui::DragDropTypes::DragEventSource)

IPC_STRUCT_TRAITS_BEGIN(content::DragEventSourceInfo)
  IPC_STRUCT_TRAITS_MEMBER(event_location)
  IPC_STRUCT_TRAITS_MEMBER(event_source)
IPC_STRUCT_TRAITS_END()
