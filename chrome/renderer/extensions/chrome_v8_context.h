// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/renderer/extensions/module_system.h"
#include "chrome/renderer/extensions/request_sender.h"
#include "chrome/renderer/extensions/scoped_persistent.h"
#include "v8/include/v8.h"

namespace WebKit {
class WebFrame;
}

namespace content {
class RenderView;
}

namespace extensions {
class Extension;

// Chrome's wrapper for a v8 context.
//
// TODO(aa): Consider converting this back to a set of bindings_utils. It would
// require adding WebFrame::GetIsolatedWorldIdByV8Context() to WebCore, but then
// we won't need this object and it's a bit less state to keep track of.
class ChromeV8Context : public RequestSender::Source {
 public:
  ChromeV8Context(v8::Handle<v8::Context> context,
                  WebKit::WebFrame* frame,
                  const Extension* extension,
                  Feature::Context context_type);
  virtual ~ChromeV8Context();

  // Clears the WebFrame for this contexts and invalidates the associated
  // ModuleSystem.
  void Invalidate();

  v8::Handle<v8::Context> v8_context() const {
    return v8_context_.get();
  }

  const Extension* extension() const {
    return extension_.get();
  }

  WebKit::WebFrame* web_frame() const {
    return web_frame_;
  }

  Feature::Context context_type() const {
    return context_type_;
  }

  void set_module_system(scoped_ptr<ModuleSystem> module_system) {
    module_system_ = module_system.Pass();
  }

  ModuleSystem* module_system() { return module_system_.get(); }

  // Returns the ID of the extension associated with this context, or empty
  // string if there is no such extension.
  std::string GetExtensionID();

  // Returns a special Chrome-specific hidden object that is associated with a
  // context, but not reachable from the JavaScript in that context. This is
  // used by our v8::Extension implementations as a way to share code and as a
  // bridge between C++ and JavaScript.
  static v8::Handle<v8::Value> GetOrCreateChromeHidden(
      v8::Handle<v8::Context> context);

  // Return the chromeHidden object associated with this context, or an empty
  // handle if no chrome hidden has been created (by GetOrCreateChromeHidden)
  // yet for this context.
  v8::Handle<v8::Value> GetChromeHidden() const;

  // Returns the RenderView associated with this context. Can return NULL if the
  // context is in the process of being destroyed.
  content::RenderView* GetRenderView() const;

  // Fires the onload and onunload events on the chromeHidden object.
  // TODO(aa): Move this to EventBindings.
  void DispatchOnLoadEvent(bool is_incognito_process, int manifest_version);
  void DispatchOnUnloadEvent();

  // Call the named method of the chromeHidden object in this context.
  // The function can be a sub-property like "Port.dispatchOnMessage". Returns
  // the result of the function call in |result| if |result| is non-NULL. If the
  // named method does not exist, returns false.
  bool CallChromeHiddenMethod(
      const std::string& function_name,
      int argc,
      v8::Handle<v8::Value>* argv,
      v8::Handle<v8::Value>* result) const;

  // Returns the availability of the API |api_name|.
  Feature::Availability GetAvailability(const std::string& api_name);

  // Returns a string description of the type of context this is.
  std::string GetContextTypeDescription();

  // RequestSender::Source implementation.
  virtual ChromeV8Context* GetContext() OVERRIDE;
  virtual void OnResponseReceived(const std::string& name,
                                  int request_id,
                                  bool success,
                                  const base::ListValue& response,
                                  const std::string& error) OVERRIDE;

 private:
  // The v8 context the bindings are accessible to.
  ScopedPersistent<v8::Context> v8_context_;

  // The WebFrame associated with this context. This can be NULL because this
  // object can outlive is destroyed asynchronously.
  WebKit::WebFrame* web_frame_;

  // The extension associated with this context, or NULL if there is none. This
  // might be a hosted app in the case that this context is hosting a web URL.
  scoped_refptr<const Extension> extension_;

  // The type of context.
  Feature::Context context_type_;

  // Owns and structures the JS that is injected to set up extension bindings.
  scoped_ptr<ModuleSystem> module_system_;

  DISALLOW_COPY_AND_ASSIGN(ChromeV8Context);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_H_
