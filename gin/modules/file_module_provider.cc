// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/file_module_provider.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "gin/converter.h"

namespace gin {

namespace {

void AttempToLoadModule(const base::WeakPtr<Runner>& runner,
                        const base::FilePath& base,
                        const std::string& id) {
  if (!runner)
    return;

  std::vector<std::string> components;
  base::SplitString(id, '/', &components);
  base::FilePath path = base;
  for (size_t i = 0; i < components.size(); ++i) {
    // TODO(abarth): Technically the path components can be UTF-8. We don't
    // handle that case correctly yet.
    path = path.AppendASCII(components[i]);
  }
  path = path.AddExtension(FILE_PATH_LITERAL("js"));

  std::string source;
  if (!ReadFileToString(path, &source))
    return;

  Runner::Scope scope(runner.get());
  v8::Handle<v8::Script> script = v8::Script::New(
      StringToV8(runner->isolate(), source),
      StringToV8(runner->isolate(), id));
  runner->Run(script);
}

}  // namespace

FileModuleProvider::FileModuleProvider(const base::FilePath& base)
    : base_(base) {
}

FileModuleProvider::~FileModuleProvider() {
}

void FileModuleProvider::AttempToLoadModules(
    Runner* runner, const std::set<std::string>& ids) {
  std::set<std::string> modules = ids;
  for (std::set<std::string>::const_iterator it = modules.begin();
       it != modules.end(); ++it) {
    const std::string& id = *it;
    if (attempted_ids_.count(id))
      continue;
    attempted_ids_.insert(id);
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(AttempToLoadModule, runner->GetWeakPtr(), base_, id));
  }
}

}  // namespace gin
