// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/ui/autofill/save_card_bubble_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "content/public/browser/navigation_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace autofill {

typedef SaveCardBubbleController::LegalMessageLine LegalMessageLine;
typedef SaveCardBubbleController::LegalMessageLines LegalMessageLines;

class TestSaveCardBubbleControllerImpl : public SaveCardBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(), new TestSaveCardBubbleControllerImpl(web_contents));
  }

  explicit TestSaveCardBubbleControllerImpl(content::WebContents* web_contents)
      : SaveCardBubbleControllerImpl(web_contents) {}

  void set_elapsed(base::TimeDelta elapsed) { elapsed_ = elapsed; }

  using SaveCardBubbleControllerImpl::DidNavigateMainFrame;

 protected:
  base::TimeDelta Elapsed() const override { return elapsed_; }

 private:
  base::TimeDelta elapsed_;
};

class SaveCardBubbleControllerImplTest : public BrowserWithTestWindowTest {
 public:
  SaveCardBubbleControllerImplTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    TestSaveCardBubbleControllerImpl::CreateForTesting(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  BrowserWindow* CreateBrowserWindow() override {
    return new SaveCardBubbleTestBrowserWindow();
  }

  void SetLegalMessage(const std::string& message_json) {
    scoped_ptr<base::Value> value(base::JSONReader::Read(message_json));
    ASSERT_TRUE(value);
    base::DictionaryValue* dictionary;
    ASSERT_TRUE(value->GetAsDictionary(&dictionary));
    scoped_ptr<base::DictionaryValue> legal_message =
        dictionary->CreateDeepCopy();
    controller()->ShowBubbleForUpload(base::Bind(&SaveCardCallback),
                                      legal_message.Pass());
  }

  // Returns true if lines are the same.
  bool CompareLegalMessageLines(const LegalMessageLine& a,
                                const LegalMessageLine& b) {
    if (a.text != b.text)
      return false;
    if (a.links.size() != b.links.size())
      return false;
    for (size_t i = 0; i < a.links.size(); ++i) {
      if (a.links[i].range != b.links[i].range)
        return false;
      if (a.links[i].url != b.links[i].url)
        return false;
    }
    return true;
  }

  // Returns true if messages are the same.
  bool CompareLegalMessages(const LegalMessageLines& a,
                            const LegalMessageLines& b) {
    if (a.size() != b.size())
      return false;
    for (size_t i = 0; i < a.size(); ++i) {
      if (!CompareLegalMessageLines(a[i], b[i]))
        return false;
    }
    return true;
  }

  void ShowLocalBubble() {
    controller()->ShowBubbleForLocalSave(base::Bind(&SaveCardCallback));
  }

  void ShowUploadBubble() {
    SetLegalMessage(
        "{"
        "  \"line\" : [ {"
        "     \"template\": \"This is the entire message.\""
        "  } ]"
        "}");
  }

  void CloseAndReshowBubble() {
    controller()->OnBubbleClosed();
    controller()->ReshowBubble();
  }

 protected:
  TestSaveCardBubbleControllerImpl* controller() {
    return static_cast<TestSaveCardBubbleControllerImpl*>(
        TestSaveCardBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

 private:
  class TestSaveCardBubbleView : public SaveCardBubbleView {
    void Hide() override{};
  };

  class SaveCardBubbleTestBrowserWindow : public TestBrowserWindow {
   public:
    SaveCardBubbleView* ShowSaveCreditCardBubble(
        content::WebContents* contents,
        SaveCardBubbleController* controller,
        bool user_gesture) override {
      if (!save_card_bubble_view_)
        save_card_bubble_view_.reset(new TestSaveCardBubbleView());
      return save_card_bubble_view_.get();
    }

   private:
    scoped_ptr<TestSaveCardBubbleView> save_card_bubble_view_;
  };

  static void SaveCardCallback() {}

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImplTest);
};

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_NoParameters) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "     \"template\": \"This is the entire message.\""
      "  } ]"
      "}");

  LegalMessageLine expected_line;
  expected_line.text = base::ASCIIToUTF16("This is the entire message.");
  LegalMessageLines expected = {expected_line};
  EXPECT_TRUE(
      CompareLegalMessages(expected, controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_SingleParameter) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "     \"template\": \"Panda {0}.\","
      "     \"template_parameter\": [ {"
      "        \"display_text\": \"bears are fuzzy\","
      "        \"url\": \"http://www.example.com\""
      "     } ]"
      "  } ]"
      "}");

  LegalMessageLine expected_line;
  expected_line.text = base::ASCIIToUTF16("Panda bears are fuzzy.");
  expected_line.links = {
      {{6, 21}, GURL("http://www.example.com")},
  };
  LegalMessageLines expected = {expected_line};
  EXPECT_TRUE(
      CompareLegalMessages(expected, controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_MissingUrl) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "     \"template\": \"Panda {0}.\","
      "     \"template_parameter\": [ {"
      "        \"display_text\": \"bear\""
      "     } ]"
      "  } ]"
      "}");
  // Legal message is invalid so GetLegalMessageLines() should return no lines.
  EXPECT_TRUE(CompareLegalMessages(LegalMessageLines(),
                                   controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_MissingDisplayText) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "    \"template\": \"Panda {0}.\","
      "    \"template_parameter\": [ {"
      "      \"url\": \"http://www.example.com\""
      "     } ]"
      "  } ]"
      "}");
  // Legal message is invalid so GetLegalMessageLines() should return no lines.
  EXPECT_TRUE(CompareLegalMessages(LegalMessageLines(),
                                   controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_EscapeCharacters) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "    \"template\": \"Panda '{'{0}'}' '{1}' don't $1.\","
      "    \"template_parameter\": [ {"
      "      \"display_text\": \"bears\","
      "      \"url\": \"http://www.example.com\""
      "     } ]"
      "  } ]"
      "}");

  LegalMessageLine expected_line;
  expected_line.text = base::ASCIIToUTF16("Panda {bears} {1} don't $1.");
  expected_line.links = {
      {{7, 12}, GURL("http://www.example.com")},
  };
  LegalMessageLines expected = {expected_line};
  EXPECT_TRUE(
      CompareLegalMessages(expected, controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_ConsecutiveDollarSigns) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "     \"template\": \"$$\""
      "  } ]"
      "}");

  // Consecutive dollar signs do not expand correctly (see comment in
  // ReplaceTemplatePlaceholders() in save_card_bubble_controller_impl.cc).
  // If this is fixed and this test starts to fail, please update the
  // "Caveats" section of the SaveCardBubbleControllerImpl::SetLegalMessage()
  // header file comment.
  LegalMessageLine expected_line;
  expected_line.text = base::ASCIIToUTF16("$$$");

  LegalMessageLines expected = {expected_line};
  EXPECT_TRUE(
      CompareLegalMessages(expected, controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_DollarAndParenthesis) {
  // "${" does not expand correctly (see comment in
  // ReplaceTemplatePlaceholders() in save_card_bubble_controller_impl.cc).
  // If this is fixed and this test starts to fail, please update the
  // "Caveats" section of the SaveCardBubbleControllerImpl::SetLegalMessage()
  // header file comment.
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "    \"template\": \"${0}\","
      "    \"template_parameter\": [ {"
      "      \"display_text\": \"bears\","
      "      \"url\": \"http://www.example.com\""
      "    } ]"
      "  } ]"
      "}");
  // Legal message is invalid so GetLegalMessageLines() should return no lines.
  EXPECT_TRUE(CompareLegalMessages(LegalMessageLines(),
                                   controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_MultipleParameters) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "    \"template\": \"Panda {0} like {2} eat {1}.\","
      "    \"template_parameter\": [ {"
      "      \"display_text\": \"bears\","
      "      \"url\": \"http://www.example.com/0\""
      "    }, {"
      "      \"display_text\": \"bamboo\","
      "      \"url\": \"http://www.example.com/1\""
      "    }, {"
      "      \"display_text\": \"to\","
      "      \"url\": \"http://www.example.com/2\""
      "    } ]"
      "  } ]"
      "}");

  LegalMessageLine expected_line;
  expected_line.text = base::ASCIIToUTF16("Panda bears like to eat bamboo.");
  expected_line.links = {
      {{6, 11}, GURL("http://www.example.com/0")},
      {{24, 30}, GURL("http://www.example.com/1")},
      {{17, 19}, GURL("http://www.example.com/2")},
  };
  LegalMessageLines expected = {expected_line};
  EXPECT_TRUE(
      CompareLegalMessages(expected, controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_MultipleLineElements) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "    \"template\": \"Panda {0}\","
      "    \"template_parameter\": [ {"
      "      \"display_text\": \"bears\","
      "      \"url\": \"http://www.example.com/line_0_param_0\""
      "    } ]"
      "  }, {"
      "    \"template\": \"like {1} eat {0}.\","
      "    \"template_parameter\": [ {"
      "      \"display_text\": \"bamboo\","
      "      \"url\": \"http://www.example.com/line_1_param_0\""
      "    }, {"
      "      \"display_text\": \"to\","
      "      \"url\": \"http://www.example.com/line_1_param_1\""
      "    } ]"
      "  }, {"
      "    \"template\": \"The {0}.\","
      "    \"template_parameter\": [ {"
      "      \"display_text\": \"end\","
      "      \"url\": \"http://www.example.com/line_2_param_0\""
      "    } ]"
      "  } ]"
      "}");

  // Line 0.
  LegalMessageLine expected_line_0;
  expected_line_0.text = base::ASCIIToUTF16("Panda bears");
  expected_line_0.links = {
      {{6, 11}, GURL("http://www.example.com/line_0_param_0")},
  };

  // Line 1.
  LegalMessageLine expected_line_1;
  expected_line_1.text = base::ASCIIToUTF16("like to eat bamboo.");
  expected_line_1.links = {
      {{12, 18}, GURL("http://www.example.com/line_1_param_0")},
      {{5, 7}, GURL("http://www.example.com/line_1_param_1")},
  };

  // Line 2.
  LegalMessageLine expected_line_2;
  expected_line_2.text = base::ASCIIToUTF16("The end.");
  expected_line_2.links = {
      {{4, 7}, GURL("http://www.example.com/line_2_param_0")},
  };

  LegalMessageLines expected = {expected_line_0, expected_line_1,
                                expected_line_2};
  EXPECT_TRUE(
      CompareLegalMessages(expected, controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_EmbeddedNewlines) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "    \"template\": \"Panda {0}\nlike {2} eat {1}.\nThe {3}.\","
      "    \"template_parameter\": [ {"
      "      \"display_text\": \"bears\","
      "      \"url\": \"http://www.example.com/0\""
      "    }, {"
      "      \"display_text\": \"bamboo\","
      "      \"url\": \"http://www.example.com/1\""
      "    }, {"
      "      \"display_text\": \"to\","
      "      \"url\": \"http://www.example.com/2\""
      "    }, {"
      "      \"display_text\": \"end\","
      "      \"url\": \"http://www.example.com/3\""
      "    } ]"
      "  } ]"
      "}");

  LegalMessageLine expected_line;
  expected_line.text =
      base::ASCIIToUTF16("Panda bears\nlike to eat bamboo.\nThe end.");
  expected_line.links = {
      {{6, 11}, GURL("http://www.example.com/0")},
      {{24, 30}, GURL("http://www.example.com/1")},
      {{17, 19}, GURL("http://www.example.com/2")},
      {{36, 39}, GURL("http://www.example.com/3")},
  };
  LegalMessageLines expected = {expected_line};
  EXPECT_TRUE(
      CompareLegalMessages(expected, controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, LegalMessage_MaximumPlaceholders) {
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "    \"template\": \"a{0} b{1} c{2} d{3} e{4} f{5} g{6}\","
      "    \"template_parameter\": [ {"
      "      \"display_text\": \"A\","
      "      \"url\": \"http://www.example.com/0\""
      "    }, {"
      "      \"display_text\": \"B\","
      "      \"url\": \"http://www.example.com/1\""
      "    }, {"
      "      \"display_text\": \"C\","
      "      \"url\": \"http://www.example.com/2\""
      "    }, {"
      "      \"display_text\": \"D\","
      "      \"url\": \"http://www.example.com/3\""
      "    }, {"
      "      \"display_text\": \"E\","
      "      \"url\": \"http://www.example.com/4\""
      "    }, {"
      "      \"display_text\": \"F\","
      "      \"url\": \"http://www.example.com/5\""
      "    }, {"
      "      \"display_text\": \"G\","
      "      \"url\": \"http://www.example.com/6\""
      "    } ]"
      "  } ]"
      "}");

  LegalMessageLine expected_line;
  expected_line.text = base::ASCIIToUTF16("aA bB cC dD eE fF gG");
  expected_line.links = {
      {{1, 2}, GURL("http://www.example.com/0")},
      {{4, 5}, GURL("http://www.example.com/1")},
      {{7, 8}, GURL("http://www.example.com/2")},
      {{10, 11}, GURL("http://www.example.com/3")},
      {{13, 14}, GURL("http://www.example.com/4")},
      {{16, 17}, GURL("http://www.example.com/5")},
      {{19, 20}, GURL("http://www.example.com/6")},
  };
  LegalMessageLines expected = {expected_line};
  EXPECT_TRUE(
      CompareLegalMessages(expected, controller()->GetLegalMessageLines()));
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_FirstShow_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_Reshows_ShowBubble) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.Reshows"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_FirstShow_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_Reshows_ShowBubble) {
  ShowUploadBubble();

  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.Reshows"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_FirstShow_SaveButton) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  controller()->OnSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_Reshows_SaveButton) {
  ShowLocalBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_FirstShow_CancelButton) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  controller()->OnCancelButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_Reshows_CancelButton) {
  ShowLocalBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnCancelButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_NavigateWhileShowing) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  // Fake-navigate after bubble has been visible for a long time.
  controller()->set_elapsed(base::TimeDelta::FromMinutes(1));
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                     content::FrameNavigateParams());

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_Reshows_NavigateWhileShowing) {
  ShowLocalBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  // Fake-navigate after bubble has been visible for a long time.
  controller()->set_elapsed(base::TimeDelta::FromMinutes(1));
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                     content::FrameNavigateParams());

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_NavigateWhileHidden) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  controller()->set_elapsed(base::TimeDelta::FromMinutes(1));
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                     content::FrameNavigateParams());

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_Reshows_NavigateWhileHidden) {
  ShowLocalBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  controller()->set_elapsed(base::TimeDelta::FromMinutes(1));
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                     content::FrameNavigateParams());

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_FirstShow_LearnMore) {
  ShowUploadBubble();

  base::HistogramTester histogram_tester;
  controller()->OnLearnMoreClicked();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEARN_MORE, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_Reshows_LearnMore) {
  ShowUploadBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnLearnMoreClicked();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEARN_MORE, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_LegalMessageLink) {
  ShowUploadBubble();

  base::HistogramTester histogram_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_LegalMessageLink) {
  ShowUploadBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE, 1);
}

// SAVE_CARD_PROMPT_END_INVALID_LEGAL_MESSAGE is only possible for
// Upload.FirstShow.
TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_InvalidLegalMessage) {
  base::HistogramTester histogram_tester;

  // Legal message is invalid because it's missing the url.
  SetLegalMessage(
      "{"
      "  \"line\" : [ {"
      "     \"template\": \"Panda {0}.\","
      "     \"template_parameter\": [ {"
      "        \"display_text\": \"bear\""
      "     } ]"
      "  } ]"
      "}");

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.FirstShow"),
      ElementsAre(
          Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
          Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_INVALID_LEGAL_MESSAGE,
                 1)));
}

}  // namespace autofill
