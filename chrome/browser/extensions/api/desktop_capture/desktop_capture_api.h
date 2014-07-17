// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_API_H_

#include <map>

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/media/desktop_media_list.h"
#include "chrome/browser/media/desktop_media_picker.h"
#include "chrome/browser/media/native_desktop_media_list.h"
#include "chrome/common/extensions/api/desktop_capture.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace extensions {

class DesktopCaptureChooseDesktopMediaFunction
    : public ChromeAsyncExtensionFunction,
      public content::WebContentsObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("desktopCapture.chooseDesktopMedia",
                             DESKTOPCAPTURE_CHOOSEDESKTOPMEDIA)

  // Factory creating DesktopMediaList and DesktopMediaPicker instances.
  // Used for tests to supply fake picker.
  class PickerFactory {
   public:
    virtual scoped_ptr<DesktopMediaList> CreateModel(bool show_screens,
                                                     bool show_windows) = 0;
    virtual scoped_ptr<DesktopMediaPicker> CreatePicker() = 0;
   protected:
    virtual ~PickerFactory() {}
  };

  // Used to set PickerFactory used to create mock DesktopMediaPicker instances
  // for tests. Calling tests keep ownership of the factory. Can be called with
  // |factory| set to NULL at the end of the test.
  static void SetPickerFactoryForTests(PickerFactory* factory);

  DesktopCaptureChooseDesktopMediaFunction();

  void Cancel();

 private:
  virtual ~DesktopCaptureChooseDesktopMediaFunction();

  // ExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

  // content::WebContentsObserver overrides.
  virtual void WebContentsDestroyed() OVERRIDE;

  void OnPickerDialogResults(content::DesktopMediaID source);

  int request_id_;

  // URL of page that desktop capture was requested for.
  GURL origin_;

  scoped_ptr<DesktopMediaPicker> picker_;
};

class DesktopCaptureCancelChooseDesktopMediaFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("desktopCapture.cancelChooseDesktopMedia",
                             DESKTOPCAPTURE_CANCELCHOOSEDESKTOPMEDIA)

  DesktopCaptureCancelChooseDesktopMediaFunction();

 private:
  virtual ~DesktopCaptureCancelChooseDesktopMediaFunction();

  // ExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

class DesktopCaptureRequestsRegistry {
 public:
  DesktopCaptureRequestsRegistry();
  ~DesktopCaptureRequestsRegistry();

  static DesktopCaptureRequestsRegistry* GetInstance();

  void AddRequest(int process_id,
                  int request_id,
                  DesktopCaptureChooseDesktopMediaFunction* handler);
  void RemoveRequest(int process_id, int request_id);
  void CancelRequest(int process_id, int request_id);

 private:
  friend struct DefaultSingletonTraits<DesktopCaptureRequestsRegistry>;

  struct RequestId {
    RequestId(int process_id, int request_id);

    // Need to use RequestId as a key in std::map<>.
    bool operator<(const RequestId& other) const;

    int process_id;
    int request_id;
  };

  typedef std::map<RequestId,
                   DesktopCaptureChooseDesktopMediaFunction*> RequestsMap;

  RequestsMap requests_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCaptureRequestsRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DESKTOP_CAPTURE_DESKTOP_CAPTURE_API_H_
