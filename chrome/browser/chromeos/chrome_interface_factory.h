// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_INTERFACE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_INTERFACE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/public/common/connection_filter.h"

namespace chromeos {

// InterfaceFactory for creating all services provided by chrome.
class ChromeInterfaceFactory : public content::ConnectionFilter {
 public:
  ChromeInterfaceFactory();
  ~ChromeInterfaceFactory() override;

 private:
  // content::ConnectionFilter:
  bool OnConnect(shell::Connection* connection,
                 shell::Connector* connector) override;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ChromeInterfaceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_INTERFACE_FACTORY_H_
