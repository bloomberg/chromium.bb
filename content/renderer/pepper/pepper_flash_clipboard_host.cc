// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_flash_clipboard_host.h"

#include "base/pickle.h"
#include "base/utf_string_conversions.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/renderer_clipboard_client.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "webkit/glue/clipboard_client.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"

namespace content {

namespace {

const size_t kMaxClipboardWriteSize = 1000000;

ui::Clipboard::Buffer ConvertClipboardType(uint32_t type) {
  switch (type) {
    case PP_FLASH_CLIPBOARD_TYPE_STANDARD:
      return ui::Clipboard::BUFFER_STANDARD;
    case PP_FLASH_CLIPBOARD_TYPE_SELECTION:
      return ui::Clipboard::BUFFER_SELECTION;
  }
  NOTREACHED();
  return ui::Clipboard::BUFFER_STANDARD;
}

// Functions to pack/unpack custom data from a pickle. See the header file for
// more detail on custom formats in Pepper.
// TODO(raymes): Currently pepper custom formats are stored in their own
// native format type. However we should be able to store them in the same way
// as "Web Custom" formats are. This would allow clipboard data to be shared
// between pepper applications and web applications. However currently web apps
// assume all data that is placed on the clipboard is UTF16 and pepper allows
// arbitrary data so this change would require some reworking of the chrome
// clipboard interface for custom data.
bool JumpToFormatInPickle(const string16& format, PickleIterator* iter) {
  uint64 size = 0;
  if (!iter->ReadUInt64(&size))
    return false;
  for (uint64 i = 0; i < size; ++i) {
    string16 stored_format;
    if (!iter->ReadString16(&stored_format))
      return false;
    if (stored_format == format)
      return true;
    int skip_length;
    if (!iter->ReadLength(&skip_length))
      return false;
    if (!iter->SkipBytes(skip_length))
      return false;
  }
  return false;
}

bool IsFormatAvailableInPickle(const string16& format, const Pickle& pickle) {
  PickleIterator iter(pickle);
  return JumpToFormatInPickle(format, &iter);
}

std::string ReadDataFromPickle(const string16& format, const Pickle& pickle) {
  std::string result;
  PickleIterator iter(pickle);
  if (!JumpToFormatInPickle(format, &iter) || !iter.ReadString(&result))
    return std::string();
  return result;
}

bool WriteDataToPickle(const std::map<string16, std::string>& data,
                       Pickle* pickle) {
  pickle->WriteUInt64(data.size());
  for (std::map<string16, std::string>::const_iterator it = data.begin();
       it != data.end(); ++it) {
    if (!pickle->WriteString16(it->first))
      return false;
    if (!pickle->WriteString(it->second))
      return false;
  }
  return true;
}

}  // namespace

PepperFlashClipboardHost::PepperFlashClipboardHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      clipboard_client_(new RendererClipboardClient) {
}

PepperFlashClipboardHost::~PepperFlashClipboardHost() {
}

int32_t PepperFlashClipboardHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashClipboardHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FlashClipboard_RegisterCustomFormat,
        OnRegisterCustomFormat);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FlashClipboard_IsFormatAvailable,
        OnIsFormatAvailable);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FlashClipboard_ReadData,
        OnReadData);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FlashClipboard_WriteData,
        OnWriteData);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashClipboardHost::OnRegisterCustomFormat(
    ppapi::host::HostMessageContext* host_context,
    const std::string& format_name) {
  uint32_t format = custom_formats_.RegisterFormat(format_name);
  if (format == PP_FLASH_CLIPBOARD_FORMAT_INVALID)
    return PP_ERROR_FAILED;
  host_context->reply_msg =
      PpapiPluginMsg_FlashClipboard_RegisterCustomFormatReply(format);
  return PP_OK;
}

int32_t PepperFlashClipboardHost::OnIsFormatAvailable(
    ppapi::host::HostMessageContext* host_context,
    uint32_t clipboard_type,
    uint32_t format) {
  if (clipboard_type != PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

  ui::Clipboard::Buffer buffer_type = ConvertClipboardType(clipboard_type);
  bool available = false;
  switch (format) {
    case PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT: {
      bool plain = clipboard_client_->IsFormatAvailable(
          ui::Clipboard::GetPlainTextFormatType(), buffer_type);
      bool plainw = clipboard_client_->IsFormatAvailable(
          ui::Clipboard::GetPlainTextWFormatType(), buffer_type);
      available = plain || plainw;
      break;
    }
    case PP_FLASH_CLIPBOARD_FORMAT_HTML:
      available = clipboard_client_->IsFormatAvailable(
          ui::Clipboard::GetHtmlFormatType(), buffer_type);
      break;
    case PP_FLASH_CLIPBOARD_FORMAT_RTF:
      available = clipboard_client_->IsFormatAvailable(
          ui::Clipboard::GetRtfFormatType(), buffer_type);
      break;
    case PP_FLASH_CLIPBOARD_FORMAT_INVALID:
      break;
    default:
      if (custom_formats_.IsFormatRegistered(format)) {
        std::string format_name = custom_formats_.GetFormatName(format);
        std::string clipboard_data;
        clipboard_client_->ReadData(
            ui::Clipboard::GetPepperCustomDataFormatType(), &clipboard_data);
        Pickle pickle(clipboard_data.data(), clipboard_data.size());
        available = IsFormatAvailableInPickle(UTF8ToUTF16(format_name), pickle);
      }
      break;
  }

  return available ? PP_OK : PP_ERROR_FAILED;
}

int32_t PepperFlashClipboardHost::OnReadData(
    ppapi::host::HostMessageContext* host_context,
    uint32_t clipboard_type,
    uint32_t format) {
  if (clipboard_type != PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

  ui::Clipboard::Buffer buffer_type = ConvertClipboardType(clipboard_type);
  std::string clipboard_string;
  int32_t result = PP_ERROR_FAILED;
  switch (format) {
    case PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT: {
      if (clipboard_client_->IsFormatAvailable(
          ui::Clipboard::GetPlainTextWFormatType(), buffer_type)) {
        string16 text;
        clipboard_client_->ReadText(buffer_type, &text);
        if (!text.empty()) {
          result = PP_OK;
          clipboard_string = UTF16ToUTF8(text);
          break;
        }
      }
      // If the PlainTextW format isn't available or is empty, take the
      // ASCII text format.
      if (clipboard_client_->IsFormatAvailable(
          ui::Clipboard::GetPlainTextFormatType(), buffer_type)) {
        result = PP_OK;
        clipboard_client_->ReadAsciiText(buffer_type, &clipboard_string);
      }
      break;
    }
    case PP_FLASH_CLIPBOARD_FORMAT_HTML: {
      if (!clipboard_client_->IsFormatAvailable(
          ui::Clipboard::GetHtmlFormatType(), buffer_type)) {
        break;
      }

      string16 html;
      GURL gurl;
      uint32 fragment_start;
      uint32 fragment_end;
      clipboard_client_->ReadHTML(buffer_type, &html, &gurl, &fragment_start,
                                  &fragment_end);
      result = PP_OK;
      clipboard_string = UTF16ToUTF8(
          html.substr(fragment_start, fragment_end - fragment_start));
      break;
    }
    case PP_FLASH_CLIPBOARD_FORMAT_RTF: {
      if (!clipboard_client_->IsFormatAvailable(
          ui::Clipboard::GetRtfFormatType(), buffer_type)) {
        break;
      }
      result = PP_OK;
      clipboard_client_->ReadRTF(buffer_type, &clipboard_string);
      break;
    }
    case PP_FLASH_CLIPBOARD_FORMAT_INVALID:
      break;
    default: {
      if (custom_formats_.IsFormatRegistered(format)) {
        string16 format_name = UTF8ToUTF16(
            custom_formats_.GetFormatName(format));
        std::string clipboard_data;
        clipboard_client_->ReadData(
            ui::Clipboard::GetPepperCustomDataFormatType(), &clipboard_data);
        Pickle pickle(clipboard_data.data(), clipboard_data.size());
        if (IsFormatAvailableInPickle(format_name, pickle)) {
          result = PP_OK;
          clipboard_string = ReadDataFromPickle(format_name, pickle);
        }
      }
      break;
    }
  }

  if (result == PP_OK) {
    host_context->reply_msg =
        PpapiPluginMsg_FlashClipboard_ReadDataReply(clipboard_string);
  }
  return result;
}

int32_t PepperFlashClipboardHost::OnWriteData(
    ppapi::host::HostMessageContext* host_context,
    uint32_t clipboard_type,
    const std::vector<uint32_t>& formats,
    const std::vector<std::string>& data) {
  if (clipboard_type != PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

  DCHECK(formats.size() == data.size());
  // If no formats are passed in clear the clipboard.
  if (formats.size() == 0) {
    clipboard_client_->Clear(ConvertClipboardType(clipboard_type));
    return PP_OK;
  }

  webkit_glue::ScopedClipboardWriterGlue scw(clipboard_client_.get());
  std::map<string16, std::string> custom_data_map;
  int32_t res = PP_OK;
  for (uint32_t i = 0; i < formats.size(); ++i) {
    if (data[i].length() > kMaxClipboardWriteSize) {
      res = PP_ERROR_NOSPACE;
      break;
    }

    switch (formats[i]) {
      case PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT:
        scw.WriteText(UTF8ToUTF16(data[i]));
        break;
      case PP_FLASH_CLIPBOARD_FORMAT_HTML:
        scw.WriteHTML(UTF8ToUTF16(data[i]), "");
        break;
      case PP_FLASH_CLIPBOARD_FORMAT_RTF:
        scw.WriteRTF(data[i]);
        break;
      case PP_FLASH_CLIPBOARD_FORMAT_INVALID:
        res = PP_ERROR_BADARGUMENT;
        break;
      default:
        if (custom_formats_.IsFormatRegistered(formats[i])) {
          std::string format_name = custom_formats_.GetFormatName(formats[i]);
          custom_data_map[UTF8ToUTF16(format_name)] = data[i];
        } else {
          // Invalid format.
          res = PP_ERROR_BADARGUMENT;
          break;
        }
    }

    if (res != PP_OK)
      break;
  }

  if (custom_data_map.size() > 0) {
    Pickle pickle;
    if (WriteDataToPickle(custom_data_map, &pickle)) {
      scw.WritePickledData(pickle,
                           ui::Clipboard::GetPepperCustomDataFormatType());
    } else {
      res = PP_ERROR_BADARGUMENT;
    }
  }

  if (res != PP_OK) {
    // Need to clear the objects so nothing is written.
    scw.Reset();
  }

  return res;
}

}  // namespace content

