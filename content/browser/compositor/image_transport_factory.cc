// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/image_transport_factory.h"

#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "content/browser/compositor/no_transport_image_transport_factory.h"
#include "ui/compositor/compositor.h"

namespace content {

namespace {
ImageTransportFactory* g_factory = NULL;
bool g_initialized_for_unit_tests = false;
}

// static
void ImageTransportFactory::Initialize() {
  DCHECK(!g_factory || g_initialized_for_unit_tests);
  if (g_initialized_for_unit_tests)
    return;
  g_factory = new GpuProcessTransportFactory;
  ui::ContextFactory::SetInstance(g_factory->AsContextFactory());
}

void ImageTransportFactory::InitializeForUnitTests(
    scoped_ptr<ui::ContextFactory> test_factory) {
  DCHECK(!g_factory);
  DCHECK(!g_initialized_for_unit_tests);
  g_initialized_for_unit_tests = true;
  g_factory = new NoTransportImageTransportFactory(test_factory.Pass());
  ui::ContextFactory::SetInstance(g_factory->AsContextFactory());
}

// static
void ImageTransportFactory::Terminate() {
  ui::ContextFactory::SetInstance(NULL);
  delete g_factory;
  g_factory = NULL;
  g_initialized_for_unit_tests = false;
}

// static
ImageTransportFactory* ImageTransportFactory::GetInstance() {
  return g_factory;
}

}  // namespace content
