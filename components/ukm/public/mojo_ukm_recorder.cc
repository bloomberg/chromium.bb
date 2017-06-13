// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/public/mojo_ukm_recorder.h"

#include "base/memory/ptr_util.h"

namespace ukm {

MojoUkmRecorder::MojoUkmRecorder(mojom::UkmRecorderInterfacePtr interface)
    : interface_(std::move(interface)) {}
MojoUkmRecorder::~MojoUkmRecorder() = default;

void MojoUkmRecorder::UpdateSourceURL(SourceId source_id, const GURL& url) {
  interface_->UpdateSourceURL(source_id, url.spec());
}

void MojoUkmRecorder::AddEntry(mojom::UkmEntryPtr entry) {
  interface_->AddEntry(std::move(entry));
}

}  // namespace ukm
