// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_MANAGER_H_
#define CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_MANAGER_H_

#include <vector>

class Profile;
class ServiceProcessControl;

// ServiceProcessControlManager is a registrar for all ServiceProcess created
// in the browser process. It is also a factory for creating new
// ServiceProcess.
class ServiceProcessControlManager {
 public:
  typedef std::vector<ServiceProcessControl*> ServiceProcessControlList;

  ServiceProcessControlManager();
  ~ServiceProcessControlManager();

  // Get the ServiceProcess instance corresponding to |profile| and |type|.
  // If such an instance doesn't exist a new instance is created.
  //
  // There will be at most one ServiceProcess for a |profile|.
  //
  // This method should only be accessed on the UI thread.
  ServiceProcessControl* GetProcessControl(Profile* profile);

  // Destroy all ServiceProcess objects created.
  void Shutdown();

  // Return the instance of ServiceProcessControlManager.
  static ServiceProcessControlManager* GetInstance();

 private:
  ServiceProcessControlList process_control_list_;
};

#endif  // CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_MANAGER_H_
