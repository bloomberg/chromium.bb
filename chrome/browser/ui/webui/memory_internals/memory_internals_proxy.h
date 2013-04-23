// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEMORY_INTERNALS_MEMORY_INTERNALS_PROXY_H_
#define CHROME_BROWSER_UI_WEBUI_MEMORY_INTERNALS_MEMORY_INTERNALS_PROXY_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/memory_details.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class ListValue;
class Value;
}

class MemoryInternalsHandler;

class MemoryInternalsProxy
    : public base::RefCountedThreadSafe<
        MemoryInternalsProxy, content::BrowserThread::DeleteOnUIThread> {
 public:
  MemoryInternalsProxy();

  // Register a handler and start receiving callbacks from memory internals.
  void Attach(MemoryInternalsHandler* handler);

  // Unregister the same and stop receiving callbacks.
  void Detach();

  // Have memory internal clients send all the data they have.
  void GetInfo(const base::ListValue* list);

 private:
  friend struct
      content::BrowserThread::DeleteOnThread<content::BrowserThread::UI>;
  friend class base::DeleteHelper<MemoryInternalsProxy>;
  virtual ~MemoryInternalsProxy();

  // Sends a message from IO thread to update UI on UI thread.
  void UpdateUIOnUIThread(const string16& update);

  // Convert memory information into DictionaryValue format.
  void OnDetailsAvailable(const ProcessData& browser);

  // Call a JavaScript function on the page. Takes ownership of |args|.
  void CallJavaScriptFunctionOnUIThread(const std::string& function,
                                        base::Value* args);

  MemoryInternalsHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(MemoryInternalsProxy);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEMORY_INTERNALS_MEMORY_INTERNALS_PROXY_H_
