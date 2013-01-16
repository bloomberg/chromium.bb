// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_SET_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_SET_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "v8/include/v8.h"

class GURL;

namespace base {
class ListValue;
}

namespace content {
class RenderView;
}

namespace v8 {
class Context;
}

namespace extensions {
class ChromeV8Context;

// A container of ExtensionBindingsContext. Since calling JavaScript within a
// context can cause any number of contexts to be created or destroyed, this
// has additional smarts to help with the set changing underneath callers.
class ChromeV8ContextSet {
 public:
  ChromeV8ContextSet();
  ~ChromeV8ContextSet();

  int size() const;

  // Takes ownership of |context|.
  void Add(ChromeV8Context* context);

  // If the specified context is contained in this set, remove it, then delete
  // it asynchronously. After this call returns the context object will still
  // be valid, but its frame() pointer will be cleared.
  void Remove(ChromeV8Context* context);

  // Returns a copy to protect against changes.
  typedef std::set<ChromeV8Context*> ContextSet;
  ContextSet GetAll() const;

  // Gets the ChromeV8Context corresponding to the v8::Context that is
  // on the top of the stack, or NULL if no such context exists.
  ChromeV8Context* GetCurrent() const;

  // Gets the ChromeV8Context corresponding to the specified
  // v8::Context or NULL if no such context exists.
  ChromeV8Context* GetByV8Context(
      v8::Handle<v8::Context> context) const;

  // Calls chromeHidden.<methodName> in each context for <extension_id>. If
  // render_view is non-NULL, only call the function in contexts belonging to
  // that view. The called javascript function should not return a value other
  // than v8::Undefined(). A DCHECK is setup to break if it is otherwise.
  void DispatchChromeHiddenMethod(const std::string& extension_id,
                                  const std::string& method_name,
                                  const base::ListValue& arguments,
                                  content::RenderView* render_view,
                                  const GURL& event_url) const;

  // Cleans up contexts belonging to an unloaded extension.
  void OnExtensionUnloaded(const std::string& extension_id);

 private:
  ContextSet contexts_;

  DISALLOW_COPY_AND_ASSIGN(ChromeV8ContextSet);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_SET_H_
