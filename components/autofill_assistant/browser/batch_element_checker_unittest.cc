// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <map>
#include <set>

#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;

namespace autofill_assistant {

namespace {

static constexpr base::TimeDelta kTimeUnit =
    base::TimeDelta::FromMilliseconds(100);

class BatchElementCheckerTest : public testing::Test {
 protected:
  BatchElementCheckerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        checks_(&mock_web_controller_) {}

  void OnElementExistenceCheck(const std::string& name, bool result) {
    element_exists_results_[name] = result;
  }

  base::OnceCallback<void(bool)> ElementExistenceCallback(
      const std::string& name) {
    return base::BindOnce(&BatchElementCheckerTest::OnElementExistenceCheck,
                          base::Unretained(this), name);
  }

  void OnFieldValueCheck(const std::string& name,
                         bool exists,
                         const std::string& value) {
    get_field_value_results_[name] = value;
  }

  base::OnceCallback<void(bool, const std::string&)> FieldValueCallback(
      const std::string& name) {
    return base::BindOnce(&BatchElementCheckerTest::OnFieldValueCheck,
                          base::Unretained(this), name);
  }

  void OnDone(const std::string& name) { all_done_.insert(name); }

  base::OnceCallback<void()> DoneCallback(const std::string& name) {
    return base::BindOnce(&BatchElementCheckerTest::OnDone,
                          base::Unretained(this), name);
  }

  void OnTry(const std::string& name) { ++try_done_[name]; }

  base::RepeatingCallback<void()> TryCallback(const std::string& name) {
    return base::BindRepeating(&BatchElementCheckerTest::OnTry,
                               base::Unretained(this), name);
  }

  void AdvanceTime() { scoped_task_environment_.FastForwardBy(kTimeUnit); }

  void RunOnce(const std::string& callback_name) {
    checks_.Run(base::TimeDelta::FromMilliseconds(0), base::DoNothing(),
                DoneCallback(callback_name));
  }

  // scoped_task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  MockWebController mock_web_controller_;
  BatchElementChecker checks_;
  std::map<std::string, bool> element_exists_results_;
  std::map<std::string, std::string> get_field_value_results_;
  std::set<std::string> all_done_;
  std::map<std::string, int> try_done_;
};

TEST_F(BatchElementCheckerTest, OneElementFound) {
  EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("exists"), _))
      .WillOnce(RunOnceCallback<1>(true));
  checks_.AddElementExistenceCheck({"exists"},
                                   ElementExistenceCallback("exists"));
  RunOnce("run_once");

  EXPECT_THAT(element_exists_results_, Contains(Pair("exists", true)));
  EXPECT_THAT(all_done_, Contains("run_once"));
  EXPECT_TRUE(checks_.all_found());
}

TEST_F(BatchElementCheckerTest, OneElementNotFound) {
  EXPECT_CALL(mock_web_controller_,
              OnElementExists(ElementsAre("does_not_exist"), _))
      .WillOnce(RunOnceCallback<1>(false));
  checks_.AddElementExistenceCheck({"does_not_exist"},
                                   ElementExistenceCallback("does_not_exist"));
  RunOnce("run_once");

  EXPECT_THAT(element_exists_results_, Contains(Pair("does_not_exist", false)));
  EXPECT_THAT(all_done_, Contains("run_once"));
  EXPECT_FALSE(checks_.all_found());
}

TEST_F(BatchElementCheckerTest, OneFieldValueFound) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(ElementsAre("field"), _))
      .WillOnce(RunOnceCallback<1>(true, "some value"));
  checks_.AddFieldValueCheck({"field"}, FieldValueCallback("field"));
  RunOnce("run_once");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", "some value")));
  EXPECT_THAT(all_done_, Contains("run_once"));
  EXPECT_TRUE(checks_.all_found());
}

TEST_F(BatchElementCheckerTest, OneFieldValueNotFound) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(ElementsAre("field"), _))
      .WillOnce(RunOnceCallback<1>(false, ""));
  checks_.AddFieldValueCheck({"field"}, FieldValueCallback("field"));
  RunOnce("run_once");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", "")));
  EXPECT_THAT(all_done_, Contains("run_once"));
  EXPECT_FALSE(checks_.all_found());
}

TEST_F(BatchElementCheckerTest, OneFieldValueEmpty) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(ElementsAre("field"), _))
      .WillOnce(RunOnceCallback<1>(true, ""));
  checks_.AddFieldValueCheck({"field"}, FieldValueCallback("field"));
  RunOnce("run_once");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", "")));
  EXPECT_THAT(all_done_, Contains("run_once"));
  EXPECT_TRUE(checks_.all_found());
}

TEST_F(BatchElementCheckerTest, MultipleElements) {
  EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("1"), _))
      .WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("2"), _))
      .WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("3"), _))
      .WillOnce(RunOnceCallback<1>(false));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(ElementsAre("4"), _))
      .WillOnce(RunOnceCallback<1>(true, "value"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(ElementsAre("5"), _))
      .WillOnce(RunOnceCallback<1>(false, ""));

  checks_.AddElementExistenceCheck({"1"}, ElementExistenceCallback("1"));
  checks_.AddElementExistenceCheck({"2"}, ElementExistenceCallback("2"));
  checks_.AddElementExistenceCheck({"3"}, ElementExistenceCallback("3"));
  checks_.AddFieldValueCheck({"4"}, FieldValueCallback("4"));
  checks_.AddFieldValueCheck({"5"}, FieldValueCallback("5"));
  RunOnce("run_once");

  EXPECT_THAT(element_exists_results_, Contains(Pair("1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("2", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("3", false)));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("4", "value")));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("5", "")));
  EXPECT_THAT(all_done_, Contains("run_once"));
  EXPECT_FALSE(checks_.all_found());
}

TEST_F(BatchElementCheckerTest, Deduplicate) {
  EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("1"), _))
      .WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(ElementsAre("2"), _))
      .WillOnce(RunOnceCallback<1>(true, "value2"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(ElementsAre("3"), _))
      .WillOnce(RunOnceCallback<1>(true, "value3"));

  // Deduplicate two AddElementExistenceCheck for the same element:
  checks_.AddElementExistenceCheck({"1"}, ElementExistenceCallback("first 1"));
  checks_.AddElementExistenceCheck({"1"}, ElementExistenceCallback("second 1"));

  // Deduplicate an AddElementExistenceCheck and a AddFieldValueCheck for the
  // same element:
  checks_.AddElementExistenceCheck({"2"}, ElementExistenceCallback("first 2"));
  checks_.AddFieldValueCheck({"2"}, FieldValueCallback("second 2"));

  // Deduplicate two AddFieldValueCheck for the same element:
  checks_.AddFieldValueCheck({"3"}, FieldValueCallback("first 3"));
  checks_.AddFieldValueCheck({"3"}, FieldValueCallback("second 3"));

  RunOnce("run_once");

  EXPECT_THAT(element_exists_results_, Contains(Pair("first 1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("second 1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("first 2", true)));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("second 2", "value2")));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("first 3", "value3")));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("second 3", "value3")));
  EXPECT_THAT(all_done_, Contains("run_once"));
}

TEST_F(BatchElementCheckerTest, EventuallyFindAll) {
  {
    InSequence seq;

    EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("1"), _))
        .WillOnce(RunOnceCallback<1>(true));
    EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("2"), _))
        .WillOnce(RunOnceCallback<1>(false))
        .WillOnce(RunOnceCallback<1>(true));
  }
  checks_.AddElementExistenceCheck({"1"}, ElementExistenceCallback("1"));
  checks_.AddElementExistenceCheck({"2"}, ElementExistenceCallback("2"));
  checks_.Run(base::TimeDelta::FromSeconds(1), base::DoNothing(),
              DoneCallback("all_done"));

  // The first try should have run, not fully successful, and should now be
  // waiting for the second try.
  EXPECT_TRUE(scoped_task_environment_.MainThreadHasPendingTask());
  EXPECT_THAT(element_exists_results_, Contains(Pair("1", true)));
  EXPECT_THAT(element_exists_results_, Not(Contains(Key("2"))));
  EXPECT_THAT(all_done_, Not(Contains("all_done")));
  EXPECT_FALSE(checks_.all_found());

  // The second try should have found 2 and finished.
  AdvanceTime();
  EXPECT_FALSE(scoped_task_environment_.MainThreadHasPendingTask());
  EXPECT_THAT(element_exists_results_, Contains(Pair("2", true)));
  EXPECT_THAT(all_done_, Contains("all_done"));
  EXPECT_TRUE(checks_.all_found());
}

TEST_F(BatchElementCheckerTest, EventuallyFindSome) {
  {
    InSequence seq;

    EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("1"), _))
        .WillOnce(RunOnceCallback<1>(true));
    EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("2"), _))
        .Times(3)
        .WillRepeatedly(RunOnceCallback<1>(false));
  }
  checks_.AddElementExistenceCheck({"1"}, ElementExistenceCallback("1"));
  checks_.AddElementExistenceCheck({"2"}, ElementExistenceCallback("2"));
  checks_.Run(3 * kTimeUnit, base::DoNothing(), DoneCallback("all_done"));

  // The first try should have run, not fully successful, and should now be
  // waiting for the second try.
  EXPECT_TRUE(scoped_task_environment_.MainThreadHasPendingTask());
  EXPECT_THAT(element_exists_results_, Contains(Pair("1", true)));
  EXPECT_THAT(element_exists_results_, Not(Contains(Key("2"))));
  EXPECT_THAT(all_done_, Not(Contains("all_done")));

  // The second try still doesn't work'
  AdvanceTime();
  EXPECT_TRUE(scoped_task_environment_.MainThreadHasPendingTask());
  EXPECT_THAT(element_exists_results_, Not(Contains(Key("2"))));

  // Give up after the third try
  AdvanceTime();
  EXPECT_FALSE(scoped_task_environment_.MainThreadHasPendingTask());
  EXPECT_THAT(element_exists_results_, Contains(Pair("2", false)));
  EXPECT_THAT(all_done_, Contains("all_done"));
  EXPECT_FALSE(checks_.all_found());
}

TEST_F(BatchElementCheckerTest, TryDoneCallback) {
  EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("element"), _))
      .WillOnce(RunOnceCallback<1>(false))
      .WillOnce(RunOnceCallback<1>(true));

  checks_.AddElementExistenceCheck({"element"}, base::DoNothing());
  checks_.Run(base::TimeDelta::FromSeconds(1), TryCallback("try"),
              DoneCallback("all_done"));

  // The first try does not fully succeed.
  EXPECT_THAT(try_done_, Contains(Pair("try", 1)));
  EXPECT_THAT(all_done_, Not(Contains("all_done")));

  // The second try succeeds and ends the run.
  AdvanceTime();
  EXPECT_THAT(try_done_, Contains(Pair("try", 2)));
  EXPECT_THAT(all_done_, Contains("all_done"));
}

TEST_F(BatchElementCheckerTest, TryOnceGivenSmallDuration) {
  EXPECT_CALL(mock_web_controller_,
              OnElementExists(ElementsAre("does_not_exist"), _))
      .WillOnce(RunOnceCallback<1>(false));
  checks_.AddElementExistenceCheck({"does_not_exist"},
                                   ElementExistenceCallback("does_not_exist"));

  checks_.Run(base::TimeDelta::FromMilliseconds(10), base::DoNothing(),
              DoneCallback("all_done"));

  EXPECT_FALSE(scoped_task_environment_.MainThreadHasPendingTask());
  EXPECT_THAT(element_exists_results_, Contains(Pair("does_not_exist", false)));
  EXPECT_THAT(all_done_, Contains("all_done"));
  EXPECT_FALSE(checks_.all_found());
}

TEST_F(BatchElementCheckerTest, StopTrying) {
  EXPECT_CALL(mock_web_controller_, OnElementExists(ElementsAre("element"), _))
      .WillRepeatedly(RunOnceCallback<1>(false));

  checks_.AddElementExistenceCheck({"element"}, base::DoNothing());
  checks_.Run(base::TimeDelta::FromSeconds(1), TryCallback("try"),
              DoneCallback("all_done"));

  // The first try does not fully succeed.
  EXPECT_THAT(try_done_, Contains(Pair("try", 1)));
  EXPECT_THAT(all_done_, Not(Contains("all_done")));

  checks_.StopTrying();

  // Give up on the second try, because of StopTrying.
  AdvanceTime();
  EXPECT_THAT(try_done_, Contains(Pair("try", 2)));
  EXPECT_THAT(all_done_, Contains("all_done"));
}

}  // namespace
}  // namespace autofill_assistant
