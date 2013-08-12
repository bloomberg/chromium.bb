// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_protocol_constants.h"

namespace content {
namespace devtools {

namespace Inspector {
namespace detached {
  const char kName[] = "Inspector.detached";
  const char kParamReason[] = "reason";
}  // detached
namespace targetCrashed {
  const char kName[] = "Inspector.targetCrashed";
}  // targetCrashed
}  // Inspector

namespace DOM {
namespace setFileInputFiles {
  const char kName[] = "DOM.setFileInputFiles";
  const char kParamFiles[] = "files";
}  // setFileInputFiles
}  // DOM

namespace Page {
namespace handleJavaScriptDialog {
  const char kName[] = "Page.handleJavaScriptDialog";
  const char kParamAccept[] = "accept";
  const char kParamPromptText[] = "promptText";
}  // handleJavaScriptDialog
namespace navigate {
  const char kName[] = "Page.navigate";
  const char kParamUrl[] = "url";
}  // navigate
namespace captureScreenshot {
  const char kName[] = "Page.captureScreenshot";
  const char kParamFormat[] = "format";
  const char kParamQuality[] = "quality";
  const char kParamScale[] = "scale";
  const char kResponseData[] = "data";
}  // captureScreenshot
}  // Page

namespace Worker {
namespace disconnectedFromWorker {
  const char kName[] = "Worker.disconnectedFromWorker";
}  // disconnectedFromWorker
}  // Worker

namespace Tracing {
  const char kName[] = "Tracing";

namespace start {
  const char kName[] = "Tracing.start";
  const char kCategories[] = "categories";
  const char kTraceOptions[] = "trace-options";
}  // start

namespace end {
  const char kName[] = "Tracing.end";
}

namespace tracingComplete {
  const char kName[] = "Tracing.tracingComplete";
}

namespace dataCollected {
  const char kName[] = "Tracing.dataCollected";
  const char kValue[] = "value";
}
}  // Tracing

}  // devtools
}  // content

