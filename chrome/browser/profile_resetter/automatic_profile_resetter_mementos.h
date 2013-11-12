// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The classes in this file are alternative implementations of the concept of a
// "prompt memento", a token of some kind that gets stored when we show the
// one-time profile reset prompt, and which then serves as a reminder that we
// should not show the prompt again.
//
// In an ideal world, a single implementation would suffice, however, we expect
// that third party software might accidentally interfere with some of these
// methods. We need this redundancy because we want to make absolutely sure that
// we do not annoy the user with the prompt multiple times.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_MEMENTOS_H_
#define CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_MEMENTOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"

namespace base {
class FilePath;
}

class Profile;

// This class is a wrapper around the user preference that gets stored when we
// show the one-time profile reset prompt, and which is kept as a reminder that
// we should not show the prompt again.
class PreferenceHostedPromptMemento {
 public:
  explicit PreferenceHostedPromptMemento(Profile* profile);
  ~PreferenceHostedPromptMemento();

  std::string ReadValue() const;
  void StoreValue(const std::string& value);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceHostedPromptMemento);
};

// This class is a wrapper around the Local State preference that gets stored
// when we show the one-time profile reset prompt, and which is kept as a
// reminder that we should not show the prompt again.
class LocalStateHostedPromptMemento {
 public:
  explicit LocalStateHostedPromptMemento(Profile* profile);
  ~LocalStateHostedPromptMemento();

  std::string ReadValue() const;
  void StoreValue(const std::string& value);

 private:
  // Returns the key that shall be used in the dictionary preference in Local
  // State to uniquely identify this profile.
  std::string GetProfileKey() const;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LocalStateHostedPromptMemento);
};

// This class manages a marker file that gets stored when we show the one-time
// profile reset prompt, and which is kept as a reminder that we should not show
// the prompt again.
class FileHostedPromptMemento {
 public:
  typedef base::Callback<void(const std::string&)> ReadValueCallback;

  explicit FileHostedPromptMemento(Profile* profile);
  ~FileHostedPromptMemento();

  // Posts to the FILE thread to read the value, then returns the value to the
  // calling thread. It is safe to destroy this object as soon as this method
  // (synchronously) returns.
  void ReadValue(const ReadValueCallback& callback) const;

  // Asynchronously stores the value on the FILE thread. However, it is safe to
  // destroy this object as soon as this method (synchronously) returns.
  void StoreValue(const std::string& value);

 private:
  static std::string ReadValueOnFileThread(
      const base::FilePath& memento_file_path);
  static void StoreValueOnFileThread(const base::FilePath& memento_file_path,
                                     const std::string& value);

  // Returns the path to the file that shall be used to store this kind of
  // memento for this profile.
  base::FilePath GetMementoFilePath() const;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FileHostedPromptMemento);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_MEMENTOS_H_
