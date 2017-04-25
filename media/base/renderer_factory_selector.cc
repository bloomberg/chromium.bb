// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/renderer_factory_selector.h"

#include "base/logging.h"

namespace media {

RendererFactorySelector::RendererFactorySelector() {}

RendererFactorySelector::~RendererFactorySelector() {}

void RendererFactorySelector::AddFactory(
    FactoryType type,
    std::unique_ptr<RendererFactory> factory) {
  DCHECK(!factories_[type]);

  factories_[type] = std::move(factory);
}

void RendererFactorySelector::SetBaseFactoryType(FactoryType type) {
  DCHECK(factories_[type]);
  base_factory_type_ = type;
}

RendererFactory* RendererFactorySelector::GetCurrentFactory() {
  DCHECK(base_factory_type_);
  FactoryType next_factory_type = base_factory_type_.value();

  RendererFactory* factory = factories_[next_factory_type].get();

  if (factory == nullptr) {
    NOTREACHED();
    return nullptr;
  }

  DVLOG(1) << __func__ << " Selected factory type: " << next_factory_type;

  return factory;
}

}  // namespace media
