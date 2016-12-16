// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ICON_LOADER_H_
#define CHROME_BROWSER_ICON_LOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/image/image.h"

////////////////////////////////////////////////////////////////////////////////
//
// A facility to read a file containing an icon asynchronously in the IO
// thread. Returns the icon in the form of an ImageSkia.
//
////////////////////////////////////////////////////////////////////////////////
class IconLoader : public base::RefCountedThreadSafe<IconLoader> {
 public:
  // An IconGroup is a class of files that all share the same icon. For all
  // platforms but Windows, and for most files on Windows, it is the file type
  // (e.g. all .mp3 files share an icon, all .html files share an icon). On
  // Windows, for certain file types (.exe, .dll, etc), each file of that type
  // is assumed to have a unique icon. In that case, each of those files is a
  // group to itself.
  using IconGroup = base::FilePath::StringType;

  enum IconSize {
    SMALL = 0,  // 16x16
    NORMAL,     // 32x32
    LARGE,      // Windows: 32x32, Linux: 48x48, Mac: Unsupported
    ALL,        // All sizes available
  };

  class Delegate {
   public:
    // Invoked when an icon has been read. |source| is the IconLoader. If the
    // icon has been successfully loaded, |result| is non-null. |group| is the
    // determined group from the original requested path.
    virtual void OnImageLoaded(IconLoader* source,
                               std::unique_ptr<gfx::Image> result,
                               const IconGroup& group) = 0;

   protected:
    virtual ~Delegate() {}
  };

  IconLoader(const base::FilePath& file_path,
             IconSize size,
             Delegate* delegate);

  // Start reading the icon on the file thread.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<IconLoader>;

  virtual ~IconLoader();

  // Given a file path, get the group for the given file.
  static IconGroup GroupForFilepath(const base::FilePath& file_path);

  // The thread ReadIcon() should be called on.
  static content::BrowserThread::ID ReadIconThreadID();

  void ReadGroup();
  void OnReadGroup();
  void ReadIcon();

  void NotifyDelegate();

  // The task runner object of the thread in which we notify the delegate.
  scoped_refptr<base::SingleThreadTaskRunner> target_task_runner_;

  base::FilePath file_path_;

  IconGroup group_;

  IconSize icon_size_;

  std::unique_ptr<gfx::Image> image_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(IconLoader);
};

#endif  // CHROME_BROWSER_ICON_LOADER_H_
