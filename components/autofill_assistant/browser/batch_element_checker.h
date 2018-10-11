// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace autofill_assistant {
class WebController;

// Helper for checking a set of elements at the same time. It avoids duplicate
// checks and supports retries.
//
// Single check:
//
// The simplest way of using a BatchElementChecker is to:
// - create an instance
// - call AddElementExistenceCheck() and AddFieldValueCheck()
// - call Run() with duration set to 0.
//
// The result of the checks is reported to the callbacks passed to
// AddElementExistenceCheck() and AddFieldValueCheck(), then the callback passed
// to Run() is called, to report the end of the a run.
//
// Check with retries:
//
// To check for existence more than once, call Run() with a duration that
// specifies how long you're willing to wait. In that mode, elements that are
// found are reported immediately. Elements that are not found are reported at
// the end, once the specified deadline has passed, just before giving up and
// calling the callback passed to Run().
//
// TODO(crbug.com/806868): Integrate into WebController and make it the only way
// of checking elements.
class BatchElementChecker {
 public:
  explicit BatchElementChecker(WebController* web_controller);
  virtual ~BatchElementChecker();

  // Checks whether the element given by |selectors| exists on the web page.
  //
  // New element existence checks cannot be added once Run has been called.
  void AddElementExistenceCheck(const std::vector<std::string>& selectors,
                                base::OnceCallback<void(bool)> callback);

  // Gets the value of |selectors| and return the result through |callback|. The
  // returned value will be the empty string in case of error or empty value.
  //
  // New field checks cannot be added once Run has been called.
  void AddFieldValueCheck(
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(bool, const std::string&)> callback);

  // Runs the checks until all elements exist or for |duration|, whichever one
  // comes first. Elements found are reported as soon as they're founds.
  // Elements not found are reported right before |all_done| is run.
  //
  // |duration| can be 0. In this case the checks are run once, without waiting.
  // |try_done| is run at the end of each try.
  void Run(const base::TimeDelta& duration,
           base::RepeatingCallback<void()> try_done,
           base::OnceCallback<void()> all_done);

  // Makes any pending Run stop after the end of the current try.
  void StopTrying() { stopped_ = true; }

  // Returns true if all element that were asked for have been found. Can be
  // called while Run is progress or afterwards.
  bool all_found() { return all_found_; }

 private:
  // Checks to run on a specific element, and the callbacks to report them to.
  struct ElementCallbacks {
    ElementCallbacks();
    ~ElementCallbacks();

    // All the callbacks registered for that element by separate calls to
    // AddElementExistenceCheck with the same selector.
    std::vector<base::OnceCallback<void(bool)>> element_exists_callbacks;

    // All the callbacks registered for that field by separate calls to
    // AddFieldValueCheck with the same selector.
    std::vector<base::OnceCallback<void(bool, const std::string&)>>
        get_field_value_callbacks;
  };

  using ElementCallbackMap =
      std::map<std::vector<std::string>, ElementCallbacks>;

  // Tries running the checks, reporting only successes.
  //
  // Calls |try_done_callback| at the end of the run.
  void Try(base::OnceCallback<void()> try_done_callback);

  // If there are still callbacks not called by a previous call to Try, call
  // them now. When this method returns, all callbacks are guaranteed to have
  // been run.
  void GiveUp();

  void OnTryDone(int64_t remaining_attempts,
                 base::RepeatingCallback<void()> try_done,
                 base::OnceCallback<void()> all_done);
  void RunSequentially(ElementCallbackMap::iterator iter);
  void OnElementExists(ElementCallbackMap::iterator iter, bool exists);
  void OnGetFieldValue(ElementCallbackMap::iterator iter,
                       bool exists,
                       const std::string& value);
  void RunElementExistsCallbacks(ElementCallbacks* element, bool result);
  void RunGetFieldValueCallbacks(ElementCallbacks* element,
                                 bool exists,
                                 const std::string& value);
  bool HasChecksToRun(const ElementCallbacks& element);
  bool HasMoreChecksToRun();

  WebController* const web_controller_;
  ElementCallbackMap element_callback_map_;
  bool all_found_;
  bool stopped_;

  // The callback built for Try(). It is kept around while going through
  // |element_callback_map_| and called once all selectors in that map have been
  // looked up once, whether that lookup was successful or not. Also used to
  // guarantee that only one Try runs at a time.
  base::OnceCallback<void()> try_done_callback_;

  base::WeakPtrFactory<BatchElementChecker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BatchElementChecker);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_
