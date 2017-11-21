// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace {

class CredentialManagerBrowserTest : public PasswordManagerBrowserTestBase {
 public:
  CredentialManagerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebAuth);
  }

  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThread();
    // Redirect all requests to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool IsShowingAccountChooser() {
    return PasswordsModelDelegateFromWebContents(WebContents())->GetState() ==
           password_manager::ui::CREDENTIAL_REQUEST_STATE;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // To permit using webauthentication features.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  // Similarly to PasswordManagerBrowserTestBase::NavigateToFile this is a
  // wrapper around ui_test_utils::NavigateURL that waits until DidFinishLoad()
  // fires. Different to NavigateToFile this method allows passing a test_server
  // and modifications to the hostname.
  void NavigateToURL(const net::EmbeddedTestServer& test_server,
                     const std::string& hostname,
                     const std::string& relative_url) {
    NavigationObserver observer(WebContents());
    GURL url = test_server.GetURL(hostname, relative_url);
    ui_test_utils::NavigateToURL(browser(), url);
    observer.Wait();
  }

  // Triggers a call to `navigator.credentials.get` to retrieve passwords, waits
  // for success, and ASSERTs that |expect_has_results| is satisfied.
  void TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(
      content::WebContents* web_contents,
      bool expect_has_results) {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "navigator.credentials.get({password: true}).then(c => {"
        "  window.domAutomationController.send(!!c);"
        "});",
        &result));
    ASSERT_EQ(expect_has_results, result);
  }

  // Attempt to create a publicKeyCredential with an unsupported algorithm type.
  void CreatePublicKeyCredentialWithUnsupportedAlgorithmAndExpectNotSupported(
      content::WebContents* web_contents) {
    std::string result;
    std::string script =
        "navigator.credentials.create({ publicKey: {"
        "  challenge: new TextEncoder().encode('climb a mountain'),"
        "  rp: { id: '1098237235409872', name: 'Acme' },"
        "  user: { "
        "    id: '1098237235409872',"
        "    name: 'avery.a.jones@example.com',"
        "    displayName: 'Avery A. Jones', "
        "    icon: 'https://pics.acme.com/00/p/aBjjjpqPb.png'},"
        "  parameters: [{ type: 'public-key', algorithm: '123'}],"
        "  timeout: 60000,"
        "  excludeList: [] }"
        "}).catch(c => window.domAutomationController.send(c.toString()));";
    ASSERT_TRUE(
        content::ExecuteScriptAndExtractString(web_contents, script, &result));
    ASSERT_EQ(
        "NotSupportedError: Parameters for this operation are not supported.",
        result);
  }

  // Schedules a call to be made to navigator.credentials.store() in the
  // `unload` handler to save a credential with |username| and |password|.
  void ScheduleNavigatorStoreCredentialAtUnload(
      content::WebContents* web_contents,
      const char* username,
      const char* password) {
    ASSERT_TRUE(content::ExecuteScript(
        web_contents,
        base::StringPrintf(
            "window.addEventListener(\"unload\", () => {"
            "  var c = new PasswordCredential({ id: '%s', password: '%s' });"
            "  navigator.credentials.store(c);"
            "});",
            username, password)));
  }

  // Tests that when navigator.credentials.store() is called in an `unload`
  // handler before a same-RenderFrame navigation, the request is guaranteed to
  // be serviced in the context of the initial document.
  //
  // If |preestablish_mojo_pipe| is set, then the CredentialManagerClient will
  // establish the Mojo connection to the ContentCredentialManager ahead of
  // time, instead of letting the Mojo connection be established on-demand when
  // the call to store() triggered from the unload handler.
  void TestStoreInUnloadHandlerForSameSiteNavigation(
      bool preestablish_mojo_pipe) {
    // Use URLs that differ on subdomains so we can tell which one was used for
    // saving, but they still belong to the same SiteInstance, so they will be
    // renderered in the same RenderFrame (in the same process).
    const GURL a_url1 = https_test_server().GetURL("foo.a.com", "/title1.html");
    const GURL a_url2 = https_test_server().GetURL("bar.a.com", "/title2.html");

    // Navigate to a mostly empty page.
    ui_test_utils::NavigateToURL(browser(), a_url1);

    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(WebContents());

    if (preestablish_mojo_pipe) {
      EXPECT_FALSE(client->has_binding_for_credential_manager());
      ASSERT_NO_FATAL_FAILURE(
          TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(
              WebContents(), false));
      EXPECT_TRUE(client->has_binding_for_credential_manager());
    }

    // Schedule storing a credential on the `unload` event.
    ASSERT_NO_FATAL_FAILURE(ScheduleNavigatorStoreCredentialAtUnload(
        WebContents(), "user", "hunter2"));

    // Trigger a same-site navigation carried out in the same RenderFrame.
    content::RenderFrameHost* old_rfh = WebContents()->GetMainFrame();
    ui_test_utils::NavigateToURL(browser(), a_url2);
    ASSERT_EQ(old_rfh, WebContents()->GetMainFrame());

    // Ensure that the old document no longer has a Mojo connection to the
    // ContentCredentialManager, nor can it get one later.
    //
    // The sequence of events for same-RFH navigations is as follows:
    //  1.) FrameHostMsg_DidStartProvisionalLoad
    //  2.) FrameLoader::PrepareForCommit
    //  2.1) Document::Shutdown (old Document)
    //  3.) mojom::FrameHost::DidCommitProvisionalLoad (new load)
    //  ... loading ...
    //  4.) FrameHostMsg_DidStopLoading
    //  5.) content::WaitForLoadStop inside NavigateToURL returns
    //  6.) NavigateToURL returns
    //
    // After Step 2.1, the old Document cannot issue a new Mojo InterfaceRequest
    // anymore. Plus, because the AssociatedInterfaceRegistry, through which the
    // associated interface to the ContentCredentialManager is retrieved, is
    // itself Channel-associated, any InterfaceRequest messages that may have
    // been issued before or during Step 2.1, will be guaranteed to arrive to
    // the browser side before DidCommitProvisionalLoad in Step 3.
    //
    // Hence it is sufficient to check that the Mojo connection is closed now.
    EXPECT_FALSE(client->has_binding_for_credential_manager());

    // Ensure that the navigator.credentials.store() call was serviced in the
    // context of the old URL, |a_url|.
    //
    // The CredentialManager Mojo interface is Channel-associated, so message
    // ordering with legacy IPC messages is preserved. Therefore, servicing the
    // store() called from the `unload` handler, triggered from
    // FrameLoader::PrepareForCommit, will be serviced before
    // DidCommitProvisionalLoad, thus before DidFinishNavigation,
    ASSERT_TRUE(client->was_store_ever_called());

    BubbleObserver prompt_observer(WebContents());
    prompt_observer.WaitForAutomaticSavePrompt();
    ASSERT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
    prompt_observer.AcceptSavePrompt();

    WaitForPasswordStore();

    password_manager::TestPasswordStore* test_password_store =
        static_cast<password_manager::TestPasswordStore*>(
            PasswordStoreFactory::GetForProfile(
                browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                .get());

    ASSERT_EQ(1u, test_password_store->stored_passwords().size());
    autofill::PasswordForm signin_form =
        test_password_store->stored_passwords().begin()->second[0];
    EXPECT_EQ(base::ASCIIToUTF16("user"), signin_form.username_value);
    EXPECT_EQ(base::ASCIIToUTF16("hunter2"), signin_form.password_value);
    EXPECT_EQ(a_url1.GetOrigin(), signin_form.origin);
  }

  // Tests the when navigator.credentials.store() is called in an `unload`
  // handler before a cross-site transfer navigation, the request is ignored.
  //
  // If |preestablish_mojo_pipe| is set, then the CredentialManagerClient will
  // establish the Mojo connection to the ContentCredentialManager ahead of
  // time, instead of letting the Mojo connection be established on-demand when
  // the call to store() triggered from the unload handler.
  void TestStoreInUnloadHandlerForCrossSiteNavigation(
      bool preestablish_mojo_pipe) {
    const GURL a_url = https_test_server().GetURL("a.com", "/title1.html");
    const GURL b_url = https_test_server().GetURL("b.com", "/title2.html");

    // Navigate to a mostly empty page.
    ui_test_utils::NavigateToURL(browser(), a_url);

    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(WebContents());

    if (preestablish_mojo_pipe) {
      EXPECT_FALSE(client->has_binding_for_credential_manager());
      ASSERT_NO_FATAL_FAILURE(
          TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(
              WebContents(), false));
      EXPECT_TRUE(client->has_binding_for_credential_manager());
    }

    // Schedule storing a credential on the `unload` event.
    ASSERT_NO_FATAL_FAILURE(ScheduleNavigatorStoreCredentialAtUnload(
        WebContents(), "user", "hunter2"));

    // Trigger a cross-site navigation that is carried out in a new renderer,
    // and which will swap out the old RenderFrameHost.
    content::RenderFrameDeletedObserver rfh_destruction_observer(
        WebContents()->GetMainFrame());
    ui_test_utils::NavigateToURL(browser(), b_url);

    // Ensure that the navigator.credentials.store() call is never serviced.
    // The sufficient conditions for this are:
    //  -- The swapped out RFH is destroyed, so the RenderFrame cannot
    //     establish a new Mojo connection to ContentCredentialManager anymore.
    //  -- There is no already existing Mojo connection to
    //  ContentCredentialManager
    //     either, which could be used to call store() in the future.
    //  -- There have not been any calls to store() in the past.
    rfh_destruction_observer.WaitUntilDeleted();
    EXPECT_FALSE(client->has_binding_for_credential_manager());
    EXPECT_FALSE(client->was_store_ever_called());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerBrowserTest);
};

// Tests.

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       AccountChooserWithOldCredentialAndNavigation) {
  // Save credentials with 'skip_zero_click'.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("password");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");
  std::string fill_password =
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'password';";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_password));

  // Call the API to trigger the notification to the client.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "navigator.credentials.get({password: true})"
      ".then(cred => window.location = '/password/done.html')"));
  // Mojo calls from the renderer are asynchronous.
  BubbleObserver(WebContents()).WaitForAccountChooser();
  PasswordsModelDelegateFromWebContents(WebContents())
      ->ChooseCredential(
          signin_form,
          password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  // Verify that the form's 'skip_zero_click' is updated and not overwritten
  // by the autofill password manager on successful login.
  WaitForPasswordStore();
  password_manager::TestPasswordStore::PasswordMap passwords_map =
      password_store->stored_passwords();
  ASSERT_EQ(1u, passwords_map.size());
  const std::vector<autofill::PasswordForm>& passwords_vector =
      passwords_map.begin()->second;
  ASSERT_EQ(1u, passwords_vector.size());
  const autofill::PasswordForm& form = passwords_vector[0];
  EXPECT_EQ(base::ASCIIToUTF16("user"), form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("password"), form.password_value);
  EXPECT_FALSE(form.skip_zero_click);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreSavesPSLMatchedCredential) {
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  // The call to |GetURL| is needed to get the correct port.
  GURL psl_url = https_test_server().GetURL("psl.example.com", "/");

  autofill::PasswordForm signin_form;
  signin_form.signon_realm = psl_url.spec();
  signin_form.password_value = base::ASCIIToUTF16("password");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = psl_url;
  password_store->AddLogin(signin_form);

  NavigateToURL(https_test_server(), "www.example.com",
                "/password/password_form.html");

  // Call the API to trigger |get| and |store| and redirect.
  ASSERT_TRUE(
      content::ExecuteScript(RenderViewHost(),
                             "navigator.credentials.get({password: true})"
                             ".then(cred => "
                             "navigator.credentials.store(cred)"
                             ".then(cred => "
                             "window.location = '/password/done.html'))"));

  // Mojo calls from the renderer are asynchronous.
  BubbleObserver(WebContents()).WaitForAccountChooser();
  PasswordsModelDelegateFromWebContents(WebContents())
      ->ChooseCredential(
          signin_form,
          password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  // Wait for the password store before checking the prompt because it pops up
  // after the store replies.
  WaitForPasswordStore();
  BubbleObserver prompt_observer(WebContents());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
  EXPECT_FALSE(prompt_observer.IsUpdatePromptShownAutomatically());

  // There should be an entry for both psl.example.com and www.example.com.
  password_manager::TestPasswordStore::PasswordMap passwords =
      password_store->stored_passwords();
  GURL www_url = https_test_server().GetURL("www.example.com", "/");
  EXPECT_EQ(2U, passwords.size());
  EXPECT_TRUE(base::ContainsKey(passwords, psl_url.spec()));
  EXPECT_TRUE(base::ContainsKey(passwords, www_url.spec()));
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       ObsoleteHttpCredentialMovedOnMigrationToHstsSite) {
  // Add an http credential to the password store.
  GURL https_origin = https_test_server().base_url();
  ASSERT_TRUE(https_origin.SchemeIs(url::kHttpsScheme));
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  GURL http_origin = https_origin.ReplaceComponents(rep);
  autofill::PasswordForm http_form;
  http_form.signon_realm = http_origin.spec();
  http_form.origin = http_origin;
  http_form.username_value = base::ASCIIToUTF16("user");
  http_form.password_value = base::ASCIIToUTF16("12345");
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_store->AddLogin(http_form);
  WaitForPasswordStore();

  // Treat the host of the HTTPS test server as HSTS.
  AddHSTSHost(https_test_server().host_port_pair().host());

  // Navigate to HTTPS page and trigger the migration.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server().GetURL("/password/done.html"));

  // Call the API to trigger the account chooser.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(), "navigator.credentials.get({password: true})"));
  BubbleObserver(WebContents()).WaitForAccountChooser();

  // Wait for the migration logic to actually touch the password store.
  WaitForPasswordStore();
  // Only HTTPS passwords should be present.
  EXPECT_TRUE(
      password_store->stored_passwords().at(http_origin.spec()).empty());
  EXPECT_FALSE(
      password_store->stored_passwords().at(https_origin.spec()).empty());
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       AutoSigninOldCredentialAndNavigation) {
  // Save credentials with 'skip_zero_click' false.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("password");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = false;
  password_store->AddLogin(signin_form);

  // Enable 'auto signin' for the profile.
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      browser()->profile()->GetPrefs());

  NavigateToFile("/password/password_form.html");
  std::string fill_password =
  "document.getElementById('username_field').value = 'trash';"
  "document.getElementById('password_field').value = 'trash';";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_password));

  // Call the API to trigger the notification to the client.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "navigator.credentials.get({password: true})"
      ".then(cred => window.location = '/password/done.html');"));

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  BubbleObserver prompt_observer(WebContents());
  // The autofill password manager shouldn't react to the successful login
  // because it was suppressed when the site got the credential back.
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreInUnloadHandler_SameSite_OnDemandMojoPipe) {
  TestStoreInUnloadHandlerForSameSiteNavigation(
      false /* preestablish_mojo_pipe */);
}

// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreInUnloadHandler_SameSite_PreestablishedPipe) {
  TestStoreInUnloadHandlerForSameSiteNavigation(
      true /* preestablish_mojo_pipe */);
}
// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreInUnloadHandler_CrossSite_OnDemandMojoPipe) {
  TestStoreInUnloadHandlerForCrossSiteNavigation(
      false /* preestablish_mojo_pipe */);
}

// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreInUnloadHandler_CrossSite_PreestablishedPipe) {
  TestStoreInUnloadHandlerForCrossSiteNavigation(
      true /* preestablish_mojo_pipe */);
}

// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       MojoConnectionRecreatedAfterNavigation) {
  const GURL a_url1 = https_test_server().GetURL("a.com", "/title1.html");
  const GURL a_url2 = https_test_server().GetURL("a.com", "/title2.html");
  const GURL a_url2_ref = https_test_server().GetURL("a.com", "/title2.html#r");
  const GURL b_url = https_test_server().GetURL("b.com", "/title2.html#ref");

  // Enable 'auto signin' for the profile.
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      browser()->profile()->GetPrefs());

  // Navigate to a mostly empty page.
  ui_test_utils::NavigateToURL(browser(), a_url1);

  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(WebContents());

  // Store a credential, and expect it to establish the Mojo connection.
  EXPECT_FALSE(client->has_binding_for_credential_manager());
  EXPECT_FALSE(client->was_store_ever_called());

  ASSERT_TRUE(content::ExecuteScript(
      WebContents(),
      "var c = new PasswordCredential({ id: 'user', password: 'hunter2' });"
      "navigator.credentials.store(c);"));

  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForAutomaticSavePrompt();
  ASSERT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();
  WaitForPasswordStore();

  EXPECT_TRUE(client->has_binding_for_credential_manager());
  EXPECT_TRUE(client->was_store_ever_called());

  // Trigger a same-site navigation.
  content::RenderFrameHost* old_rfh = WebContents()->GetMainFrame();
  ui_test_utils::NavigateToURL(browser(), a_url2);
  ASSERT_EQ(old_rfh, WebContents()->GetMainFrame());

  // Expect the Mojo connection closed.
  EXPECT_FALSE(client->has_binding_for_credential_manager());

  // Calling navigator.credentials.get() again should re-establish the Mojo
  // connection and succeed.
  ASSERT_NO_FATAL_FAILURE(
      TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(WebContents(),
                                                               true));
  EXPECT_TRUE(client->has_binding_for_credential_manager());

  // Same-document navigation. Call to get() succeeds.
  ui_test_utils::NavigateToURL(browser(), a_url2_ref);
  ASSERT_EQ(old_rfh, WebContents()->GetMainFrame());
  EXPECT_TRUE(client->has_binding_for_credential_manager());
  ASSERT_NO_FATAL_FAILURE(
      TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(WebContents(),
                                                               true));

  // Cross-site navigation. Call to get() succeeds without results.
  ui_test_utils::NavigateToURL(browser(), b_url);
  ASSERT_NO_FATAL_FAILURE(
      TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(WebContents(),
                                                               false));

  // Trigger a cross-site navigation back. Call to get() should still succeed,
  // and once again with results.
  ui_test_utils::NavigateToURL(browser(), a_url1);
  ASSERT_NO_FATAL_FAILURE(
      TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(WebContents(),
                                                               true));
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, SaveViaAPIAndAutofill) {
  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('input_submit_button').addEventListener('click',"
      "function(event) {"
        "var c = new PasswordCredential({ id: 'user', password: 'API' });"
        "navigator.credentials.store(c);"
      "});"));
  // Fill the password and click the button to submit the page. The API should
  // suppress the autofill password manager.
  NavigationObserver form_submit_observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'autofill';"
      "document.getElementById('input_submit_button').click();"));
  form_submit_observer.Wait();

  WaitForPasswordStore();
  BubbleObserver prompt_observer(WebContents());
  ASSERT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  WaitForPasswordStore();
  password_manager::TestPasswordStore::PasswordMap stored =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get())->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  autofill::PasswordForm signin_form = stored.begin()->second[0];
  EXPECT_EQ(base::ASCIIToUTF16("user"), signin_form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("API"), signin_form.password_value);
  EXPECT_EQ(embedded_test_server()->base_url().spec(),
            signin_form.signon_realm);
  EXPECT_EQ(embedded_test_server()->base_url(), signin_form.origin);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, UpdateViaAPIAndAutofill) {
  // Save credentials with 'skip_zero_click' false.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("old_pass");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  signin_form.preferred = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('input_submit_button').addEventListener('click',"
      "function(event) {"
        "var c = new PasswordCredential({ id: 'user', password: 'API' });"
        "navigator.credentials.store(c);"
      "});"));
  // Fill the new password and click the button to submit the page later. The
  // API should suppress the autofill password manager and overwrite the
  // password.
  NavigationObserver form_submit_observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'autofill';"
      "document.getElementById('input_submit_button').click();"));
  form_submit_observer.Wait();

  // Wait for the password store before checking the prompt because it pops up
  // after the store replies.
  WaitForPasswordStore();
  BubbleObserver prompt_observer(WebContents());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
  EXPECT_FALSE(prompt_observer.IsUpdatePromptShownAutomatically());
  signin_form.skip_zero_click = false;
  signin_form.times_used = 1;
  signin_form.password_value = base::ASCIIToUTF16("API");
  password_manager::TestPasswordStore::PasswordMap stored =
      password_store->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  EXPECT_EQ(signin_form, stored[signin_form.signon_realm][0]);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, CredentialsAutofilled) {
  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecuteScript(
      RenderFrameHost(),
      "var c = new PasswordCredential({ id: 'user', password: '12345' });"
      "navigator.credentials.store(c);"));
  BubbleObserver bubble_observer(WebContents());
  bubble_observer.WaitForAutomaticSavePrompt();
  bubble_observer.AcceptSavePrompt();

  // Reload the page and make sure it's autofilled.
  NavigateToFile("/password/password_form.html");
  WaitForElementValue("username_field", "user");
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));
  WaitForElementValue("password_field", "12345");
}

// Tests that when navigator.credentials.create() is called with an unsupported
// algorithm, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       CreatePublicKeyCredentialAlgorithmNotSupported) {
  const GURL a_url1 = https_test_server().GetURL("a.com", "/title1.html");

  // Navigate to a mostly empty page.
  ui_test_utils::NavigateToURL(browser(), a_url1);

  ASSERT_NO_FATAL_FAILURE(
      CreatePublicKeyCredentialWithUnsupportedAlgorithmAndExpectNotSupported(
          WebContents()));
}

}  // namespace
