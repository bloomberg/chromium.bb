// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/weak_ptr.h"

#include "cros/chromeos_imageburn.h"

struct ImageBurnStatus {
  explicit ImageBurnStatus(const chromeos::BurnStatus& status)
      : amount_burnt(status.amount_burnt),
        total_size(status.total_size) {
    if (status.target_path)
      target_path = status.target_path;
    if (status.error)
      error = status.error;
  }
  std::string target_path;
  int64 amount_burnt;
  int64 total_size;
  std::string error;
};

namespace chromeos {
class BurnLibrary {
 public:
  class Observer {
   public:
    virtual void ProgressUpdated(BurnLibrary* object, BurnEventType evt,
                                 const ImageBurnStatus& status) = 0;
  };

  virtual ~BurnLibrary() {}

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool DoBurn(const FilePath& from_path, const FilePath& to_path) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static BurnLibrary* GetImpl(bool stub);
};

class BurnLibraryImpl : public BurnLibrary,
                        public base::SupportsWeakPtr<BurnLibraryImpl> {
 public:

  BurnLibraryImpl();
  virtual ~BurnLibraryImpl();

  // BurnLibrary implementation.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual bool DoBurn(const FilePath& from_path, const FilePath& to_path);

  bool BurnImage(const FilePath& from_path, const FilePath& to_path);
  void UpdateBurnStatus(const ImageBurnStatus& status, BurnEventType evt);

 private:
  void Init();
  static void BurnStatusChangedHandler(void* object,
                                       const BurnStatus& status,
                                       BurnEventType evt);

 private:
  ObserverList<BurnLibrary::Observer> observers_;
  BurnStatusConnection burn_status_connection_;

  // Holds a path that is currently being burnt to.
  std::string target_path_;

  DISALLOW_COPY_AND_ASSIGN(BurnLibraryImpl);
};

class BurnLibraryTaskProxy
    : public base::RefCountedThreadSafe<BurnLibraryTaskProxy> {
 public:
  explicit BurnLibraryTaskProxy(const base::WeakPtr<BurnLibraryImpl>& library);

  void BurnImage(const FilePath& from_path, const FilePath& to_path);

  void UpdateBurnStatus(ImageBurnStatus* status, BurnEventType evt);

 private:
  base::WeakPtr<BurnLibraryImpl> library_;

  friend class base::RefCountedThreadSafe<BurnLibraryTaskProxy>;

  DISALLOW_COPY_AND_ASSIGN(BurnLibraryTaskProxy);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_
