// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_CERTIFICATE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_CERTIFICATE_MANAGER_HANDLER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/options_ui.h"
#include "net/base/cert_database.h"

class CertificateManagerModel;

class CertificateManagerHandler : public OptionsPageUIHandler {
 public:
  CertificateManagerHandler();
  virtual ~CertificateManagerHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void RegisterMessages();

 private:
  // View certificate.
  void View(const ListValue* args);

  // Populate the trees in all the tabs.
  void Populate(const ListValue* args);

  // Populate the given tab's tree.
  void PopulateTree(const std::string& tab_name, net::CertType type);

  // The Certificates Manager model
  scoped_ptr<CertificateManagerModel> certificate_manager_model_;

  DISALLOW_COPY_AND_ASSIGN(CertificateManagerHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_CERTIFICATE_MANAGER_HANDLER_H_
