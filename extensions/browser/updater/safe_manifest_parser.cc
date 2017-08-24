// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/safe_manifest_parser.h"

#include <memory>

#include "base/bind.h"
#include "base/optional.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "extensions/common/manifest_parser.mojom.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using UtilityProcess =
    content::UtilityProcessMojoClient<extensions::mojom::ManifestParser>;

using ResultCallback = base::Callback<void(const UpdateManifest::Results*)>;

void ParseDone(std::unique_ptr<UtilityProcess> /* utility_process */,
               const ResultCallback& callback,
               const base::Optional<UpdateManifest::Results>& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  callback.Run(results ? &results.value() : nullptr);
}

}  // namespace

namespace extensions {

void ParseUpdateManifest(const std::string& xml,
                         const ResultCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback);

  auto process = std::make_unique<UtilityProcess>(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_MANIFEST_PARSER_NAME));
  auto* utility_process = process.get();
  auto done = base::Bind(&ParseDone, base::Passed(&process), callback);
  utility_process->set_error_callback(base::Bind(done, base::nullopt));

  utility_process->Start();

  utility_process->service()->Parse(xml, done);
}

}  // namespace extensions
