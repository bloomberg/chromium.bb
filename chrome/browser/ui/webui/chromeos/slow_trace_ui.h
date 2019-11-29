// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SLOW_TRACE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SLOW_TRACE_UI_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedString;
}

namespace chromeos {

// This class provides the source for chrome://slow_trace/.  It needs to be a
// separate handler that chrome://slow, because URLDataSource and
// WebUIDataSource are not descended from each other, and WebUIDataSource
// doesn't allow the MimeType to be dynamically specified.
class SlowTraceSource : public content::URLDataSource {
 public:
  SlowTraceSource();
  ~SlowTraceSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool AllowCaching() override;

 private:
  void OnGetTraceData(content::URLDataSource::GotDataCallback callback,
                      scoped_refptr<base::RefCountedString> trace_data);

  DISALLOW_COPY_AND_ASSIGN(SlowTraceSource);
};

class SlowTraceController : public content::WebUIController {
 public:
  explicit SlowTraceController(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(SlowTraceController);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SLOW_TRACE_UI_H_
