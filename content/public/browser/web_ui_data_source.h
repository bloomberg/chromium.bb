// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_DATA_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_DATA_SOURCE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"

namespace base {
class DictionaryValue;
class RefCountedMemory;
}

namespace content {
class BrowserContext;

// A data source that can help with implementing the common operations needed by
// WebUI pages.
class WebUIDataSource {
 public:
  virtual ~WebUIDataSource() {}

  CONTENT_EXPORT static WebUIDataSource* Create(const std::string& source_name);

  // Adds a WebUI data source to |browser_context|. TODO(dbeam): update this API
  // to take a std::unique_ptr instead to make it clear that |source| can be
  // destroyed and references should not be kept by callers. Use |Update()|
  // if you need to change an existing data source.
  CONTENT_EXPORT static void Add(BrowserContext* browser_context,
                                 WebUIDataSource* source);

  CONTENT_EXPORT static void Update(
      BrowserContext* browser_context,
      const std::string& source_name,
      std::unique_ptr<base::DictionaryValue> update);

  // Adds a string keyed to its name to our dictionary.
  virtual void AddString(base::StringPiece name,
                         const base::string16& value) = 0;

  // Adds a string keyed to its name to our dictionary.
  virtual void AddString(base::StringPiece name, const std::string& value) = 0;

  // Adds a localized string with resource |ids| keyed to its name to our
  // dictionary.
  virtual void AddLocalizedString(base::StringPiece name, int ids) = 0;

  // Add strings from |localized_strings| to our dictionary.
  virtual void AddLocalizedStrings(
      const base::DictionaryValue& localized_strings) = 0;

  // Adds a boolean keyed to its name to our dictionary.
  virtual void AddBoolean(base::StringPiece name, bool value) = 0;

  // Adds a signed 32-bit integer keyed to its name to our dictionary. Larger
  // integers may not be exactly representable in JavaScript. See
  // MAX_SAFE_INTEGER in /v8/src/globals.h.
  virtual void AddInteger(base::StringPiece name, int32_t value) = 0;

  // Sets the path which will return the JSON strings.
  virtual void SetJsonPath(base::StringPiece path) = 0;

  // Adds a mapping between a path name and a resource to return.
  virtual void AddResourcePath(base::StringPiece path, int resource_id) = 0;

  // Sets the resource to returned when no other paths match.
  virtual void SetDefaultResource(int resource_id) = 0;

  // Used as a parameter to GotDataCallback. The caller has to run this callback
  // with the result for the path that they filtered, passing ownership of the
  // memory.
  typedef base::Callback<void(scoped_refptr<base::RefCountedMemory>)>
      GotDataCallback;

  // Used by SetRequestFilter. The string parameter is the path of the request.
  // If the callee doesn't want to handle the data, false is returned. Otherwise
  // true is returned and the GotDataCallback parameter is called either then or
  // asynchronously with the response.
  typedef base::Callback<bool(const std::string&, const GotDataCallback&)>
      HandleRequestCallback;

  // Allows a caller to add a filter for URL requests.
  virtual void SetRequestFilter(const HandleRequestCallback& callback) = 0;

  // The following map to methods on URLDataSource. See the documentation there.
  // NOTE: it's not acceptable to call DisableContentSecurityPolicy for new
  // pages, see URLDataSource::ShouldAddContentSecurityPolicy and talk to
  // tsepez.

  // Currently only used by embedders for WebUIs with multiple instances.
  virtual void DisableReplaceExistingSource() = 0;
  virtual void DisableContentSecurityPolicy() = 0;
  virtual void OverrideContentSecurityPolicyScriptSrc(
      const std::string& data) = 0;
  virtual void OverrideContentSecurityPolicyObjectSrc(
      const std::string& data) = 0;
  virtual void OverrideContentSecurityPolicyChildSrc(
      const std::string& data) = 0;
  virtual void DisableDenyXFrameOptions() = 0;

  // Tells the loading code that resources are gzipped on disk.
  virtual void UseGzip() = 0;

  // Same as UseGzip() above, but |is_gzipped_callback| is used to dynamically
  // determine whether a given |const std::string& path| argument is gzipped.
  virtual void UseGzip(base::RepeatingCallback<bool(const std::string&)>
                           is_gzipped_callback) = 0;

  // The |source_name| this WebUIDataSource was created with.
  virtual std::string GetSource() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_DATA_SOURCE_H_
