// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/browser_driver/browser_driver_application_delegate.h"

#include <stdint.h>

#include "base/bind.h"
#include "components/mus/public/cpp/event_matcher.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_impl.h"

namespace mash {
namespace browser_driver {
namespace {

enum class Accelerator : uint32_t {
  NewWindow,
  NewTab,
  NewIncognitoWindow,
};

struct AcceleratorSpec {
  Accelerator id;
  mus::mojom::KeyboardCode keyboard_code;
  mus::mojom::EventFlags event_flags;
};

AcceleratorSpec g_spec[] = {
  { Accelerator::NewWindow,
    mus::mojom::KEYBOARD_CODE_N,
    mus::mojom::EVENT_FLAGS_CONTROL_DOWN },
  { Accelerator::NewTab,
    mus::mojom::KEYBOARD_CODE_T,
    mus::mojom::EVENT_FLAGS_CONTROL_DOWN },
  { Accelerator::NewIncognitoWindow,
    mus::mojom::KEYBOARD_CODE_N,
    static_cast<mus::mojom::EventFlags>(mus::mojom::EVENT_FLAGS_CONTROL_DOWN |
        mus::mojom::EVENT_FLAGS_SHIFT_DOWN) },
};

void AssertTrue(bool success) {
  DCHECK(success);
}

}  // namespace

BrowserDriverApplicationDelegate::BrowserDriverApplicationDelegate()
    : app_(nullptr),
      binding_(this) {}

BrowserDriverApplicationDelegate::~BrowserDriverApplicationDelegate() {}

void BrowserDriverApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
  AddAccelerators();
}

bool BrowserDriverApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}

void BrowserDriverApplicationDelegate::OnAccelerator(
    uint32_t id, mus::mojom::EventPtr event) {
  switch (static_cast<Accelerator>(id)) {
    case Accelerator::NewWindow:
    case Accelerator::NewTab:
    case Accelerator::NewIncognitoWindow:
      app_->ConnectToApplication("exe:chrome");
      // TODO(beng): have Chrome export a service that allows it to be driven by
      //             this driver, e.g. to open new tabs, incognito windows, etc.
      break;
    default:
      NOTREACHED();
      break;
  }
}

void BrowserDriverApplicationDelegate::AddAccelerators() {
  // TODO(beng): find some other way to get the window manager. I don't like
  //             having to specify it by URL because it may differ per display.
  mus::mojom::AcceleratorRegistrarPtr registrar;
  app_->ConnectToService("mojo:desktop_wm", &registrar);

  if (binding_.is_bound())
    binding_.Unbind();
  mus::mojom::AcceleratorHandlerPtr handler;
  binding_.Bind(GetProxy(&handler));
  // If the window manager restarts, the handler pipe will close and we'll need
  // to re-add our accelerators when the window manager comes back up.
  binding_.set_connection_error_handler(
      base::Bind(&BrowserDriverApplicationDelegate::AddAccelerators,
                 base::Unretained(this)));
  registrar->SetHandler(std::move(handler));

  for (const AcceleratorSpec& spec : g_spec) {
    registrar->AddAccelerator(
        static_cast<uint32_t>(spec.id),
        mus::CreateKeyMatcher(spec.keyboard_code, spec.event_flags),
        base::Bind(&AssertTrue));
  }
}

}  // namespace browser_driver
}  // namespace main
