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
  current_factory_needs_update_ = true;
}

// For the moment, this method should only be called once or twice.
// This method will be regularly called whenever the logic in choosing a
// renderer type is moved out of the AdaptiveRendererFactory, into this method.
void RendererFactorySelector::UpdateCurrentFactory() {
  DCHECK(base_factory_type_);
  FactoryType next_factory_type = base_factory_type_.value();

  if (use_media_player_)
    next_factory_type = FactoryType::MEDIA_PLAYER;

  DVLOG(1) << __func__ << " Selecting factory type: " << next_factory_type;

  current_factory_ = factories_[next_factory_type].get();
  current_factory_needs_update_ = false;
}

RendererFactory* RendererFactorySelector::GetCurrentFactory() {
  if (current_factory_needs_update_)
    UpdateCurrentFactory();

  DCHECK(current_factory_);
  return current_factory_;
}

#if defined(OS_ANDROID)
void RendererFactorySelector::SetUseMediaPlayer(bool use_media_player) {
  use_media_player_ = use_media_player;
  current_factory_needs_update_ = true;
}
#endif

}  // namespace media
