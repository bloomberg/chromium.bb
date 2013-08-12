// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/image_transport_factory.h"

#include "base/command_line.h"
#include "content/browser/aura/gpu_process_transport_factory.h"
#include "content/browser/aura/no_transport_image_transport_factory.h"
#include "content/public/common/content_switches.h"
#include "ui/compositor/compositor_setup.h"

namespace content {

namespace {
ImageTransportFactory* g_factory;
}

// static
void ImageTransportFactory::Initialize() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestCompositor)) {
    ui::SetupTestCompositor();
  }
  if (ui::IsTestCompositorEnabled()) {
    g_factory = new NoTransportImageTransportFactory(
        new ui::TestContextFactory);
  } else {
    g_factory = new GpuProcessTransportFactory;
  }
  ui::ContextFactory::SetInstance(g_factory->AsContextFactory());
}

// static
void ImageTransportFactory::Terminate() {
  ui::ContextFactory::SetInstance(NULL);
  delete g_factory;
  g_factory = NULL;
}

// static
ImageTransportFactory* ImageTransportFactory::GetInstance() {
  return g_factory;
}

}  // namespace content
