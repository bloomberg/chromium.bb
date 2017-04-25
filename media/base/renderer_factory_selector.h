// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_
#define MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_

#include "base/optional.h"
#include "media/base/renderer_factory.h"

namespace media {

// RendererFactorySelector owns RendererFactory instances used within WMPI.
// Its purpose is to aggregate the signals and centralize the logic behind
// choosing which RendererFactory should be used when creating a new Renderer.
class MEDIA_EXPORT RendererFactorySelector {
 public:
  enum FactoryType {
    DEFAULT,       // DefaultRendererFactory.
    MOJO,          // MojoRendererFactory.
    MEDIA_PLAYER,  // MediaPlayerRendererClientFactory.
    ADAPTIVE,      // AdaptiveRendererFactory.
    FACTORY_TYPE_MAX = ADAPTIVE,
  };

  RendererFactorySelector();
  ~RendererFactorySelector();

  // NOTE: There should be at most one factory per factory type.
  void AddFactory(FactoryType type, std::unique_ptr<RendererFactory> factory);

  // Sets the base factory to be returned, when there are no signals telling us
  // to select any specific factory.
  // NOTE: |type| can be different than FactoryType::DEFAULT. DEFAULT is used to
  // identify the DefaultRendererFactory, not to indicate that a factory should
  // be used by default.
  void SetBaseFactoryType(FactoryType type);

  // SetBaseFactoryType() must be called before calling this method.
  // NOTE: This only returns the base factory type at the moment.
  RendererFactory* GetCurrentFactory();

 private:
  base::Optional<FactoryType> base_factory_type_;
  std::unique_ptr<RendererFactory> factories_[FACTORY_TYPE_MAX + 1];
  DISALLOW_COPY_AND_ASSIGN(RendererFactorySelector);
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_FACTORY_H_
