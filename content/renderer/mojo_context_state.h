// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_CONTEXT_STATE_H_
#define CONTENT_RENDERER_MOJO_CONTEXT_STATE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "gin/modules/module_registry_observer.h"
#include "v8/include/v8.h"

namespace blink {
class WebLocalFrame;
class WebURLResponse;
}

namespace content {

class ResourceFetcher;
class MojoMainRunner;
enum class MojoBindingsType;

// MojoContextState manages the modules needed for mojo bindings. It does this
// by way of gin. Non-builtin modules are downloaded by way of ResourceFetchers.
class MojoContextState : public gin::ModuleRegistryObserver {
 public:
  MojoContextState(blink::WebLocalFrame* frame,
                   v8::Local<v8::Context> context,
                   MojoBindingsType bindings_type);
  ~MojoContextState() override;

  void Run();

  // Returns true if at least one module was added.
  bool module_added() const { return module_added_; }

 private:
  class Loader;

  // Invokes FetchModule() for any modules that have not already been
  // downloaded.
  void FetchModules(const std::vector<std::string>& ids);

  // Creates a ResourceFetcher to download |module|.
  void FetchModule(const std::string& module);

  // Callback once a module has finished downloading. Passes data to |runner_|.
  void OnFetchModuleComplete(ResourceFetcher* fetcher,
                             const std::string& id,
                             const blink::WebURLResponse& response,
                             const std::string& data);

  // gin::ModuleRegistryObserver overrides:
  void OnDidAddPendingModule(
      const std::string& id,
      const std::vector<std::string>& dependencies) override;

  // Frame script is executed in. Also used to download resources.
  blink::WebLocalFrame* frame_;

  // See description above getter.
  bool module_added_;

  // Executes the script from gin.
  std::unique_ptr<MojoMainRunner> runner_;

  // Set of fetchers we're waiting on to download script.
  std::vector<std::unique_ptr<ResourceFetcher>> module_fetchers_;

  // Set of modules we've fetched script from.
  std::set<std::string> fetched_modules_;

  // The prefix to use for all module requests.
  const std::string module_prefix_;

  DISALLOW_COPY_AND_ASSIGN(MojoContextState);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_CONTEXT_STATE_H_
