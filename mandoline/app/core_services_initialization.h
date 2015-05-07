// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_APP_CORE_SERVICES_INITIALIZATION_H_
#define MANDOLINE_APP_CORE_SERVICES_INITIALIZATION_H_

namespace mojo {
namespace runner {
class Context;
}
}

namespace mandoline {

// Sets up the aliases that forward applications to core_services.
void InitCoreServicesForContext(mojo::runner::Context* context);

}  // namespace mandoline

#endif //  MANDOLINE_APP_CORE_SERVICES_INITIALIZATION_H_
