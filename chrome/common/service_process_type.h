// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_TYPE_H_
#define CHROME_COMMON_SERVICE_PROCESS_TYPE_H_

// Defines different types of service process.
enum ServiceProcessType {
  // A service process that hosts a cloud print proxy.
  kServiceProcessCloudPrint,

  // A service process that hosts a remoting host process.
  kServiceProcessRemoting,
};

#endif  // CHROME_COMMON_SERVICE_PROCESS_TYPE_H_
