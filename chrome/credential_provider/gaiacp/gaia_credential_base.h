// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_BASE_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_BASE_H_

#include "chrome/credential_provider/gaiacp/stdafx.h"

#include <memory>

#include "base/strings/string16.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/scoped_handle.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace credential_provider {

class OSProcessManager;

enum FIELDID {
  FID_DESCRIPTION,
  FID_CURRENT_PASSWORD_FIELD,
  FID_SUBMIT,
  FID_PROVIDER_LOGO,
  FID_PROVIDER_LABEL,
  FIELD_COUNT  // Must be last.
};

// Implementation of an ICredentialProviderCredential backed by a Gaia account.
// This is used as a base class for the COM objects that implement first time
// sign in and password update.
class ATL_NO_VTABLE CGaiaCredentialBase
    : public IGaiaCredential,
      public ICredentialProviderCredential2 {
 public:
  // Size in wchar_t of string buffer to pass account information to background
  // process to save that information into the registry.
  static const int kAccountInfoBufferSize = 2048;

  // Called when the DLL is registered or unregistered.
  static HRESULT OnDllRegisterServer();
  static HRESULT OnDllUnregisterServer();

  // Saves gaia information in the OS account that was just created.
  static HRESULT SaveAccountInfo(const base::DictionaryValue& properties);

  // Allocates a BSTR from a DLL string resource given by |id|.
  static BSTR AllocErrorString(UINT id);

  // Gets the directory where the credential provider is installed.
  static HRESULT GetInstallDirectory(base::FilePath* path);

  // Passed to WaitForLoginUI().
  struct UIProcessInfo {
    UIProcessInfo();
    ~UIProcessInfo();

    CComPtr<IGaiaCredential> credential;
    base::win::ScopedHandle logon_token;
    base::win::ScopedProcessInformation procinfo;
    StdParentHandles parent_handles;
  };

 protected:
  CGaiaCredentialBase();
  ~CGaiaCredentialBase();

  // Members to access user credentials.
  const CComBSTR& get_username() const { return username_; }
  const CComBSTR& get_password() const { return password_; }
  const CComBSTR& get_sid() const { return user_sid_; }
  const CComBSTR& get_current_windows_password() const {
    return current_windows_password_;
  }
  const base::DictionaryValue* get_authentication_results() const {
    return authentication_results_.get();
  }
  void set_username(BSTR username) { username_ = username; }
  void set_user_sid(BSTR sid) { user_sid_ = sid; }
  void set_current_windows_password(BSTR password) {
    current_windows_password_ = password;
  }

  // Returns true if the current credentials stored in |username_| and
  // |password_| are valid and should succeed a local Windows logon. This
  // function is successful if we are able to create a logon token with the
  // credentials.
  bool AreCredentialsValid() const;

  // Returns true if some kind of credential information is available so that a
  // sign in can be attempled. Does not guarantee that the sign in will succeed
  // with the current credentials.
  bool CanAttemptWindowsLogon() const;

  // Returns true if the specific password credential is valid for |username_|.
  HRESULT IsWindowsPasswordValidForStoredUser(BSTR password) const;

  // Updates the UI so that the password field is displayed and also sets the
  // state of the credential to wait for a password.
  void DisplayPasswordField(int password_message);

  // IGaiaCredential
  IFACEMETHODIMP Initialize(IGaiaCredentialProvider* provider) override;
  IFACEMETHODIMP Terminate() override;
  IFACEMETHODIMP OnUserAuthenticated(BSTR authentication_info,
                                     BSTR* status_text) override;
  IFACEMETHODIMP ReportError(LONG status,
                             LONG substatus,
                             BSTR status_text) override;

  // Gets the string value for the given credential UI field.
  virtual HRESULT GetStringValueImpl(DWORD field_id, wchar_t** value);

  // Resets the state of the credential, forgetting any username or password
  // that may have been set previously.  Derived classes may override to
  // perform more state resetting if needed, but should always call the base
  // class method.
  virtual void ResetInternalState();

  // Gets the base portion of the command line to run the Gaia Logon stub.
  // This portion of the command line would only include the executable and
  // any executable specific arguments needed to correctly start the GLS.
  virtual HRESULT GetBaseGlsCommandline(base::CommandLine* command_line);

  // Gets the user specific portion of the command line to run the Gaia Logon
  // stub. This portion of the command line could include additional information
  // such as the user's email and their gaia id.
  virtual HRESULT GetUserGlsCommandline(base::CommandLine* command_line);

  // Display error message to the user.  Virtual so that tests can override.
  virtual void DisplayErrorInUI(LONG status, LONG substatus, BSTR status_text);

  // Forks a stub process to save account information for a user.
  virtual HRESULT ForkSaveAccountInfoStub(
      const std::unique_ptr<base::DictionaryValue>& dict,
      BSTR* status_text);

  // Forks the logon stub process and waits for it to start.
  virtual HRESULT ForkGaiaLogonStub(OSProcessManager* process_manager,
                                    const base::CommandLine& command_line,
                                    UIProcessInfo* uiprocinfo);

 private:
  // Gets the full command line to run the Gaia Logon stub (GLS). This
  // function calls GetBaseGlsCommandline.
  HRESULT GetGlsCommandline(base::CommandLine* command_line);

  // Called from GetSerialization() to handle auto-logon.  If the credential
  // has enough information in internal state to auto-logon, the two arguments
  // are filled in as needed and S_OK is returned.  S_FALSE is returned to
  // indicate that the UI should be shown to the user.
  HRESULT HandleAutologon(
      CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
      CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs);

  // Writes value to omaha registry to record that GCP has been used.
  static void TellOmahaDidRun();

  // Sets up the envriroment for the Gaia logon stub, runs it, and waits for
  // it to finish in a background thread.
  HRESULT CreateAndRunLogonStub();

  // Creates a restricted token for the Gaia account that can be used to run
  // the logon stub.  The returned SID is a logon SID and not the SID of the
  // Gaia account.
  static HRESULT CreateGaiaLogonToken(base::win::ScopedHandle* token,
                                      PSID* sid);

  // The param is a pointer to a UIProcessInfo struct.  This function must
  // release the memory for this structure using delete operator.
  static unsigned __stdcall WaitForLoginUI(void* param);

  // ICredentialProviderCredential2
  IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents* cpce) override;
  IFACEMETHODIMP UnAdvise(void) override;
  IFACEMETHODIMP SetSelected(BOOL* auto_login) override;
  IFACEMETHODIMP SetDeselected(void) override;
  IFACEMETHODIMP GetFieldState(
      DWORD dwFieldID,
      CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
      CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis) override;
  IFACEMETHODIMP GetStringValue(DWORD dwFieldID, wchar_t** ppsz) override;
  IFACEMETHODIMP GetBitmapValue(DWORD dwFieldID, HBITMAP* phbmp) override;
  IFACEMETHODIMP GetCheckboxValue(DWORD field_id,
                                  BOOL* pbChecked,
                                  wchar_t** ppszLabel) override;
  IFACEMETHODIMP GetSubmitButtonValue(DWORD field_id,
                                      DWORD* pdwAdjacentTo) override;
  IFACEMETHODIMP GetComboBoxValueCount(DWORD field_id,
                                       DWORD* pcItems,
                                       DWORD* pdwSelectedItem) override;
  IFACEMETHODIMP GetComboBoxValueAt(DWORD field_id,
                                    DWORD dwItem,
                                    wchar_t** ppszItem) override;
  IFACEMETHODIMP SetStringValue(DWORD field_id, const wchar_t* psz) override;
  IFACEMETHODIMP SetCheckboxValue(DWORD field_id, BOOL bChecked) override;
  IFACEMETHODIMP SetComboBoxSelectedValue(DWORD field_id,
                                          DWORD dwSelectedItem) override;
  IFACEMETHODIMP CommandLinkClicked(DWORD field_id) override;
  IFACEMETHODIMP GetSerialization(
      CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* cpgsr,
      CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs,
      wchar_t** status_text,
      CREDENTIAL_PROVIDER_STATUS_ICON* status_icon) override;
  IFACEMETHODIMP ReportResult(
      NTSTATUS ntsStatus,
      NTSTATUS ntsSubstatus,
      wchar_t** ppszOptionalStatusText,
      CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) override;
  IFACEMETHODIMP GetUserSid(wchar_t** sid) override;

  // Checks whether the submit button for the credential should be enabled
  // (depending on the state of the credential) and enables / disables it
  // accordingly. This generally occurs when processing is being done that does
  // not require direct user input to the credential (user is entering
  // credentials in  GLS) or a submit of the credential is not valid (user needs
  // to enter the old Windows password but currently nothing has been entered in
  // the password field).
  void UpdateSubmitButtonInteractiveState();

  // Stops the GLS process in case it is still executing. Often called when user
  // switches credentials in the middle of a sign in through the GLS.
  void TerminateLogonProcess();

  // Checks whether this credential can sign in the user specified by
  // |username|, |sid|, For the default anonymous gaia credential this function
  // will return S_OK. For implementers that are associated to a specific user,
  // this function should verify if the |username| and |sid| matches the
  // username and sid stored in the credential. If these verifications fail then
  // the function should return an error code which will cause sign in to fail.
  virtual HRESULT ValidateExistingUser(const base::string16& username,
                                       const base::string16& sid,
                                       BSTR* error_text);

  // Checks the information given in |result| to determine if a user can be
  // created or re-used.
  // Returns S_OK if a valid was found or a new user was created. Also allocates
  // and fills |username|, |password|, |sid| with the information for the user.
  // The caller must take ownership of this memory.
  // On failure |error_text| will be allocated and filled with an error message.
  // The caller must take ownership of this memory.
  HRESULT ValidateOrCreateUser(const base::DictionaryValue* result,
                               BSTR* username,
                               BSTR* password,
                               BSTR* sid,
                               BSTR* error_text);

  CComPtr<ICredentialProviderCredentialEvents> events_;

  // Handle to the logon UI process.
  HANDLE logon_ui_process_;

  CComPtr<IGaiaCredentialProvider> provider_;

  // Information about the just created or re-auth-ed user.
  CComBSTR username_;
  CComBSTR password_;
  CComBSTR user_sid_;

  bool needs_windows_password_ = false;
  bool needs_to_update_windows_password_ = false;

  // The password entered into the FID_CURRENT_PASSWORD_FIELD to update the
  // Windows password with the gaia password.
  CComBSTR current_windows_password_;

  std::unique_ptr<base::DictionaryValue> authentication_results_;
  // Whether success or failure, these members hold information about result.
  NTSTATUS result_status_;
  NTSTATUS result_substatus_;
  base::string16 result_status_text_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_BASE_H_
