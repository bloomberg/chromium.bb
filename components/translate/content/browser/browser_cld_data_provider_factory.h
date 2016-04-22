// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOT DEAD CODE!
// This code isn't dead, even if it isn't currently being used. Please refer to:
// https://www.chromium.org/developers/how-tos/compact-language-detector-cld-data-source-configuration

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_DATA_PROVIDER_FACTORY_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_DATA_PROVIDER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/translate/content/browser/browser_cld_data_provider.h"

namespace content {
class WebContents;
}

namespace translate {

// A factory for the Browser side of CLD Data Providers. The embedder should
// set an instance as soon as feasible during startup. The browser process will
// use this factory to create the one BrowserCldDataProvider for each render
// view host in order to communicate with a RendererCldDataProvider in the
// renderer process. For more information on the RendererCldDataProvider, see:
//   ../renderer/renderer_cld_data_provider.h
class BrowserCldDataProviderFactory {
 public:
  BrowserCldDataProviderFactory() {}
  virtual ~BrowserCldDataProviderFactory() {}

  // Create and return a new instance of a BrowserCldDataProvider. The default
  // implementation of this method produces a no-op BrowserCldDataProvider that
  // is suitable only when CLD data has been statically linked.
  // Every invocation creates a new provider; the caller is responsible for
  // deleting the object when it is no longer needed.
  virtual std::unique_ptr<BrowserCldDataProvider> CreateBrowserCldDataProvider(
      content::WebContents* web_contents);

  // Returns true if and only if the current instance for this process is not
  // NULL.
  static bool IsInitialized();

  // Sets the default factory for this process, i.e. the factory to be used
  // unless the embedder calls Set(BrowserCldDataProviderFactory*). This is the
  // method that normal (i.e., non-test) Chromium code should use; embedders can
  // and should use the unconditional Set(BrowserCldDataProviderFactory*) method
  // instead. If a default factory has already been set, this method does
  // nothing.
  static void SetDefault(BrowserCldDataProviderFactory* instance);

  // Unconditionally sets the factory for this process, overwriting any
  // previously-configured default. Normal Chromium code should never use this
  // method; it is provided for embedders to inject a factory from outside of
  // the Chromium code base. Test code can also use this method to force the
  // runtime to have a desired behavior.
  static void Set(BrowserCldDataProviderFactory* instance);

  // Returns the instance of the factory previously set by Set()/SetDefault().
  // If no instance has been set, a default factory will be returned that
  // produces no-op BrowserCldDataProviders as described by
  // CreateBrowserCldDataProvider(...) above.
  static BrowserCldDataProviderFactory* Get();

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCldDataProviderFactory);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_DATA_PROVIDER_FACTORY_H_
