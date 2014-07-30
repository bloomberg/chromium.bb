// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_DATA_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_DATA_PROVIDER_H_

#include <string>

#include "base/files/file_path.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Message;
}

namespace content {
class WebContents;
}

namespace translate {

// Browser-side interface responsible for providing CLD data.
// The implementation must be paired with a renderer-side implementation of
// the RendererCldDataProvider class:
//
//   components/translate/content/renderer/renderer_cld_data_provider.h
//
// ... and the glue between them is typically a pair of request/response IPC
// messages using the CldDataProviderMsgStart IPCMessageStart enumerated
// constant from ipc_message_start.h
class BrowserCldDataProvider : public IPC::Listener {
 public:
  virtual ~BrowserCldDataProvider() {}

  // IPC::Listener implementation:
  // If the specified message is a request for CLD data, invokes
  // OnCldDataRequest() and returns true. In all other cases, this method does
  // nothing. This method is defined as virtual in order to force the
  // implementation to define the specific IPC message(s) that it handles.
  virtual bool OnMessageReceived(const IPC::Message&) = 0;

  // Called when the browser process receives an appropriate message in
  // OnMessageReceived, above. The implementation should attempt to locate
  // the CLD data, cache any metadata required for accessing that data, and
  // ultimately trigger a response by invoking SendCldDataResponse.
  //
  // The renderer process may poll for data, in which case this method may be
  // repeatedly invoked. The implementation must be safe to call any number
  // of times.
  virtual void OnCldDataRequest() = 0;

  // Invoked when OnCldDataRequest, above, results in a successful lookup or
  // the data is already cached and ready to respond to. The implementation
  // should take whatever action is appropriate for responding to the paired
  // RendererCldDataProvider, typically by sending an IPC response.
  virtual void SendCldDataResponse() = 0;
};

// Static factory function defined by the implementation that produces a new
// provider for the specified WebContents.
BrowserCldDataProvider* CreateBrowserCldDataProviderFor(
    content::WebContents*);

// For data sources that support a separate CLD data file, configures the path
// of that data file.
//
// The 'component' and 'standalone' data sources need this method to be called
// in order to locate the CLD data on disk.
// If the data source doesn't need or doesn't support such configuration, this
// function should do nothing. This is the case for, e.g., the static data
// source.
void SetCldDataFilePath(const base::FilePath& path);

// Returns the path most recently set by SetCldDataFilePath. The initial value
// prior to any such call is the empty path. If the data source doesn't support
// a data file, returns the empty path.
base::FilePath GetCldDataFilePath();

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_DATA_PROVIDER_H_
