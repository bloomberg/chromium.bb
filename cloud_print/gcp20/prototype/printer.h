// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GCP20_PROTOTYPE_PRINTER_H_
#define GCP20_PROTOTYPE_PRINTER_H_

#include <string>
#include <vector>

#include "cloud_print/gcp20/prototype/dns_sd_server.h"

// This class will maintain work of DNS-SD server, HTTP server and others.
class Printer {
 public:
  // Constructs uninitialized object.
  Printer();

  // Destroys the object.
  ~Printer();

  // Starts all servers.
  bool Start();

  // Stops all servers.
  void Stop();

 private:
  // Creates data for DNS TXT respond.
  std::vector<std::string> CreateTxt() const;

  // Contains |true| if initialized.
  bool initialized_;

  // Contains DNS-SD server.
  DnsSdServer dns_server_;

  DISALLOW_COPY_AND_ASSIGN(Printer);
};

#endif  // GCP20_PROTOTYPE_PRINTER_H_

