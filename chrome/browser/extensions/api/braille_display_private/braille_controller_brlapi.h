// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_BRLAPI_H_
#define CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_BRLAPI_H_

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"
#include "library_loaders/libbrlapi.h"

namespace extensions {
namespace api {
namespace braille_display_private {

// BrailleController implementation based on libbrlapi from brltty.
class BrailleControllerImpl : public BrailleController {
 public:
  static BrailleControllerImpl* GetInstance();
  virtual scoped_ptr<DisplayState> GetDisplayState() OVERRIDE;
  virtual void WriteDots(const std::string& cells) OVERRIDE;
  virtual void AddObserver(BrailleObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BrailleObserver* observer) OVERRIDE;

 private:
  // For the unit tests.
  friend class BrailleDisplayPrivateApiTest;
  friend class MockBrlapiConnection;

  BrailleControllerImpl();
  virtual ~BrailleControllerImpl();
  void TryLoadLibBrlApi();

  typedef base::Callback<scoped_ptr<BrlapiConnection>()>
      CreateBrlapiConnectionFunction;

  // For dependency injection in tests.  Sets the function used to create
  // brlapi connections.
  void SetCreateBrlapiConnectionForTesting(
      const CreateBrlapiConnectionFunction& callback);

  // Makes the controller try to reconnect (if disconnected) as if the brlapi
  // socket directory had changed.
  void PokeSocketDirForTesting();

  // Tries to connect and starts watching for new brlapi servers.
  // No-op if already called.
  void StartConnecting();
  void OnSocketDirChanged(const base::FilePath& path, bool error);
  void TryToConnect();
  void Disconnect();
  scoped_ptr<BrlapiConnection> CreateBrlapiConnection();
  void DispatchKeys();
  scoped_ptr<KeyEvent> MapKeyCode(brlapi_keyCode_t code);
  void DispatchKeyEvent(scoped_ptr<KeyEvent> event);
  void DispatchOnDisplayStateChanged(scoped_ptr<DisplayState> new_state);

  LibBrlapiLoader libbrlapi_loader_;
  CreateBrlapiConnectionFunction create_brlapi_connection_function_;

  // Manipulated on the IO thread.
  scoped_ptr<BrlapiConnection> connection_;
  base::FilePathWatcher file_path_watcher_;
  bool started_connecting_;

  // Manipulated on the UI thread.
  ObserverList<BrailleObserver> observers_;

  friend struct DefaultSingletonTraits<BrailleControllerImpl>;

  DISALLOW_COPY_AND_ASSIGN(BrailleControllerImpl);
};

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_BRLAPI_H_
