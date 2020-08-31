// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_browser_font_singleton_host.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/values.h"
#include "content/common/font_list.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/resource_message_filter.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

namespace {

// Handles the font list request on the blocking pool.
class FontMessageFilter : public ppapi::host::ResourceMessageFilter {
 public:
  FontMessageFilter();

  // ppapi::host::ResourceMessageFilter implementation.
  scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) override;
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

 private:
  ~FontMessageFilter() override;

  // Message handler.
  int32_t OnHostMsgGetFontFamilies(ppapi::host::HostMessageContext* context);

  DISALLOW_COPY_AND_ASSIGN(FontMessageFilter);
};

FontMessageFilter::FontMessageFilter() {}

FontMessageFilter::~FontMessageFilter() {}

scoped_refptr<base::SequencedTaskRunner>
FontMessageFilter::OverrideTaskRunnerForMessage(const IPC::Message& msg) {
  // Use the font list SequencedTaskRunner to get the font list (currently the
  // only message) since getting the font list is non-threadsafe on Linux (for
  // versions of Pango predating 2013).
  return GetFontListTaskRunner();
}

int32_t FontMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(FontMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_BrowserFontSingleton_GetFontFamilies,
        OnHostMsgGetFontFamilies)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t FontMessageFilter::OnHostMsgGetFontFamilies(
    ppapi::host::HostMessageContext* context) {
  // OK to use "slow blocking" version since we're on the blocking pool.
  std::unique_ptr<base::ListValue> list(GetFontList_SlowBlocking());

  std::string output;
  for (size_t i = 0; i < list->GetSize(); i++) {
    base::ListValue* cur_font;
    if (!list->GetList(i, &cur_font))
      continue;

    // Each entry is actually a list of (font name, localized name).
    // We only care about the regular name.
    std::string font_name;
    if (!cur_font->GetString(0, &font_name))
      continue;

    // Font names are separated with nulls. We also want an explicit null at
    // the end of the string (Pepper strings aren't null terminated so since
    // we specify there will be a null, it should actually be in the string).
    output.append(font_name);
    output.push_back(0);
  }

  context->reply_msg =
      PpapiPluginMsg_BrowserFontSingleton_GetFontFamiliesReply(output);
  return PP_OK;
}

}  // namespace

PepperBrowserFontSingletonHost::PepperBrowserFontSingletonHost(
    BrowserPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource) {
  AddFilter(scoped_refptr<ppapi::host::ResourceMessageFilter>(
      new FontMessageFilter()));
}

PepperBrowserFontSingletonHost::~PepperBrowserFontSingletonHost() {}

}  // namespace content
