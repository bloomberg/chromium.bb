// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CERTIFICATE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CERTIFICATE_MANAGER_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/certificate_manager_model.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "net/base/nss_cert_database.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/cryptohome_client.h"
#endif

namespace options {

class CertIdMap;
class FileAccessProvider;

class CertificateManagerHandler
    : public OptionsPageUIHandler,
      public CertificateManagerModel::Observer,
      public ui::SelectFileDialog::Listener {
 public:
  CertificateManagerHandler();
  virtual ~CertificateManagerHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // CertificateManagerModel::Observer implementation.
  virtual void CertificatesRefreshed() OVERRIDE;

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

 private:
  // View certificate.
  void View(const base::ListValue* args);

  // Edit server certificate trust values.
  void EditServer(const base::ListValue* args);

  // Edit certificate authority trust values.  The sequence goes like:
  //  1. user clicks edit button -> CertificateEditCaTrustOverlay.show ->
  //  GetCATrust -> CertificateEditCaTrustOverlay.populateTrust
  //  2. user clicks ok -> EditCATrust -> CertificateEditCaTrustOverlay.dismiss
  void GetCATrust(const base::ListValue* args);
  void EditCATrust(const base::ListValue* args);

  // Cleanup state stored during import or export process.
  void CancelImportExportProcess(const base::ListValue* args);
  void ImportExportCleanup();

  // Export to PKCS #12 file.  The sequence goes like:
  //  1a. user click on export button -> ExportPersonal -> launches file
  //  selector
  //  1b. user click on export all button -> ExportAllPersonal -> launches file
  //  selector
  //  2. user selects file -> ExportPersonalFileSelected -> launches password
  //  dialog
  //  3. user enters password -> ExportPersonalPasswordSelected -> unlock slots
  //  4. slots unlocked -> ExportPersonalSlotsUnlocked -> exports to memory
  //  buffer -> starts async write operation
  //  5. write finishes (or fails) -> ExportPersonalFileWritten
  void ExportPersonal(const base::ListValue* args);
  void ExportAllPersonal(const base::ListValue* args);
  void ExportPersonalFileSelected(const FilePath& path);
  void ExportPersonalPasswordSelected(const base::ListValue* args);
  void ExportPersonalSlotsUnlocked();
  void ExportPersonalFileWritten(const int* write_errno,
                                 const int* bytes_written);

  // Import from PKCS #12 file.  The sequence goes like:
  //  1. user click on import button -> StartImportPersonal -> launches file
  //  selector
  //  2. user selects file -> ImportPersonalFileSelected -> launches password
  //  dialog
  //  3. user enters password -> ImportPersonalPasswordSelected -> starts async
  //  read operation
  //  4. read operation completes -> ImportPersonalFileRead -> unlock slot
  //  5. slot unlocked -> ImportPersonalSlotUnlocked attempts to
  //  import with previously entered password
  //  6a. if import succeeds -> ImportExportCleanup
  //  6b. if import fails -> show error, ImportExportCleanup
  //  TODO(mattm): allow retrying with different password
  void StartImportPersonal(const base::ListValue* args);
  void ImportPersonalFileSelected(const FilePath& path);
  void ImportPersonalPasswordSelected(const base::ListValue* args);
  void ImportPersonalFileRead(const int* read_errno, const std::string* data);
  void ImportPersonalSlotUnlocked();

  // Import Server certificates from file.  Sequence goes like:
  //  1. user clicks on import button -> ImportServer -> launches file selector
  //  2. user selects file -> ImportServerFileSelected -> starts async read
  //  3. read completes -> ImportServerFileRead -> parse certs -> attempt import
  //  4a. if import succeeds -> ImportExportCleanup
  //  4b. if import fails -> show error, ImportExportCleanup
  void ImportServer(const base::ListValue* args);
  void ImportServerFileSelected(const FilePath& path);
  void ImportServerFileRead(const int* read_errno, const std::string* data);

  // Import Certificate Authorities from file.  Sequence goes like:
  //  1. user clicks on import button -> ImportCA -> launches file selector
  //  2. user selects file -> ImportCAFileSelected -> starts async read
  //  3. read completes -> ImportCAFileRead -> parse certs ->
  //  CertificateEditCaTrustOverlay.showImport
  //  4. user clicks ok -> ImportCATrustSelected -> attempt import
  //  5a. if import succeeds -> ImportExportCleanup
  //  5b. if import fails -> show error, ImportExportCleanup
  void ImportCA(const base::ListValue* args);
  void ImportCAFileSelected(const FilePath& path);
  void ImportCAFileRead(const int* read_errno, const std::string* data);
  void ImportCATrustSelected(const base::ListValue* args);

  // Export a certificate.
  void Export(const base::ListValue* args);

  // Delete certificate and private key (if any).
  void Delete(const base::ListValue* args);

  // Populate the trees in all the tabs.
  void Populate(const base::ListValue* args);

  // Populate the given tab's tree.
  void PopulateTree(const std::string& tab_name, net::CertType type);

  // Display a WebUI error message box.
  void ShowError(const std::string& title, const std::string& error) const;

  // Display a WebUI error message box for import failures.
  // Depends on |selected_cert_list_| being set to the imports that we
  // attempted to import.
  void ShowImportErrors(
      const std::string& title,
      const net::NSSCertDatabase::ImportCertFailureList& not_imported) const;

#if defined(OS_CHROMEOS)
  // Check whether Tpm token is ready and notifiy JS side.
  void CheckTpmTokenReady(const base::ListValue* args);
  void CheckTpmTokenReadyInternal(
      chromeos::DBusMethodCallStatus call_status,
      bool is_tpm_token_ready);
#endif

  gfx::NativeWindow GetParentWindow() const;

  // The Certificates Manager model
  scoped_ptr<CertificateManagerModel> certificate_manager_model_;

  // For multi-step import or export processes, we need to store the path,
  // password, etc the user chose while we wait for them to enter a password,
  // wait for file to be read, etc.
  FilePath file_path_;
  string16 password_;
  bool use_hardware_backed_;
  std::string file_data_;
  net::CertificateList selected_cert_list_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  scoped_refptr<net::CryptoModule> module_;

  // Used in reading and writing certificate files.
  CancelableTaskTracker tracker_;
  scoped_refptr<FileAccessProvider> file_access_provider_;

  base::WeakPtrFactory<CertificateManagerHandler> weak_ptr_factory_;

  scoped_ptr<CertIdMap> cert_id_map_;

  DISALLOW_COPY_AND_ASSIGN(CertificateManagerHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CERTIFICATE_MANAGER_HANDLER_H_
