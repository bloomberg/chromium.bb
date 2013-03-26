// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_H_

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"

namespace base {
class DictionaryValue;
}

class Chrome;
class Status;
class WebView;

struct FrameInfo {
  FrameInfo(const std::string& parent_frame_id,
            const std::string& frame_id,
            const std::string& chromedriver_frame_id);

  std::string parent_frame_id;
  std::string frame_id;
  std::string chromedriver_frame_id;
};

struct Session {
  explicit Session(const std::string& id);
  Session(const std::string& id, scoped_ptr<Chrome> chrome);
  ~Session();

  Status GetTargetWindow(WebView** web_view);

  void SwitchToTopFrame();
  void SwitchToSubFrame(const std::string& frame_id,
                        const std::string& chromedriver_frame_id);
  std::string GetCurrentFrameId() const;

  const std::string id;
  base::Thread thread;
  scoped_ptr<Chrome> chrome;
  std::string window;
  // List of |FrameInfo|s for each frame to the current target frame from the
  // first frame element in the root document. If target frame is window.top,
  // this list will be empty.
  std::list<FrameInfo> frames;
  WebPoint mouse_position;
  int implicit_wait;
  int page_load_timeout;
  int script_timeout;
  std::string prompt_text;
  scoped_ptr<Geoposition> overridden_geoposition;
  const scoped_ptr<base::DictionaryValue> capabilities;

 private:
  scoped_ptr<base::DictionaryValue> CreateCapabilities();
};

class SessionAccessor : public base::RefCountedThreadSafe<SessionAccessor> {
 public:
  virtual Session* Access(scoped_ptr<base::AutoLock>* lock) = 0;

  // The session should be accessed before its deletion.
  virtual void DeleteSession() = 0;

 protected:
  friend class base::RefCountedThreadSafe<SessionAccessor>;
  virtual ~SessionAccessor() {}
};

class SessionAccessorImpl : public SessionAccessor {
 public:
  explicit SessionAccessorImpl(scoped_ptr<Session> session);

  virtual Session* Access(scoped_ptr<base::AutoLock>* lock) OVERRIDE;
  virtual void DeleteSession() OVERRIDE;

 private:
  virtual ~SessionAccessorImpl();

  base::Lock session_lock_;
  scoped_ptr<Session> session_;

  DISALLOW_COPY_AND_ASSIGN(SessionAccessorImpl);
};

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_H_
