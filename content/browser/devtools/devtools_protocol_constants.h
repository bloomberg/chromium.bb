// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_CONSTANTSH_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_CONSTANTSH_

// The constants in this file should be used instead manually constructing
// strings passed to and from DevTools protocol.
//
// There is a plan to generate this file from inspector.json automatically.
// Until then please feel free to add the constants here as needed.

namespace content {
namespace devtools {

namespace Inspector {
namespace detached {
  extern const char kName[];
  extern const char kParamReason[];
}  // detached
namespace targetCrashed {
  extern const char kName[];
}  // targetCrashed
}  // Inspector

namespace DOM {
namespace setFileInputFiles {
  extern const char kName[];
  extern const char kParamFiles[];
}  // setFileInputFiles
}  // DOM

namespace Page {
namespace handleJavaScriptDialog {
  extern const char kName[];
  extern const char kParamAccept[];
  extern const char kParamPromptText[];
}  // handleJavaScriptDialog
namespace navigate {
  extern const char kName[];
  extern const char kParamUrl[];
}  // navigate
namespace captureScreenshot {
  extern const char kName[];
  extern const char kParamFormat[];
  extern const char kParamQuality[];
  extern const char kParamScale[];
  extern const char kResponseData[];
}  // captureScreenshot
}  // Page

namespace Tracing {
  extern const char kName[];

namespace start {
  extern const char kName[];
  extern const char kCategories[];
  extern const char kTraceOptions[];
}  // start

namespace end {
  extern const char kName[];
}

namespace tracingComplete {
  extern const char kName[];
}

namespace dataCollected {
  extern const char kName[];
  extern const char kValue[];
}
}  // Tracing


namespace Worker {
namespace disconnectedFromWorker {
  extern const char kName[];
}  // disconnectedFromWorker
}  // Worker

}  // devtools
}  // content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_CONSTANTSH_
