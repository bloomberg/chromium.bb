// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_OBSERVER_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_OBSERVER_H_
#pragma once

// An interface that notifies the consumers about the state of the import
// operation.
class ImporterObserver {
 public:
  virtual ~ImporterObserver() {}

  // The import operation was completed successfully.
  virtual void ImportCompleted() = 0;

  // The import operation was canceled by the user.
  // TODO(4164): this is never invoked, either rip it out or invoke it.
  virtual void ImportCanceled() = 0;
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_OBSERVER_H_
