// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_ACCELERATORS_GTK_H_
#define CHROME_BROWSER_GTK_ACCELERATORS_GTK_H_

#include "app/menus/accelerator_gtk.h"
#include "base/hash_tables.h"

class AcceleratorsGtk {
 public:
  AcceleratorsGtk();
  ~AcceleratorsGtk() { }

  typedef std::vector<std::pair<int, menus::AcceleratorGtk> >
      AcceleratorGtkList;
  typedef AcceleratorGtkList::const_iterator const_iterator;

  const_iterator const begin() {
    return all_accelerators_.begin();
  }

  const_iterator const end() {
    return all_accelerators_.end();
  }

  // Returns NULL if there is no accelerator for the command.
  const menus::AcceleratorGtk* GetPrimaryAcceleratorForCommand(int command_id);

 private:
  base::hash_map<int, menus::AcceleratorGtk> primary_accelerators_;

  AcceleratorGtkList all_accelerators_;
};

#endif  // CHROME_BROWSER_GTK_ACCELERATORS_GTK_H_
