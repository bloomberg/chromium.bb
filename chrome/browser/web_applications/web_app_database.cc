// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"

namespace web_app {

WebAppDatabase::WebAppDatabase(AbstractWebAppDatabaseFactory* database_factory,
                               ReportErrorCallback error_callback)
    : database_factory_(database_factory),
      error_callback_(std::move(error_callback)) {
  DCHECK(database_factory_);
}

WebAppDatabase::~WebAppDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebAppDatabase::OpenDatabase(RegistryOpenedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!store_);

  syncer::OnceModelTypeStoreFactory store_factory =
      database_factory_->GetStoreFactory();

  std::move(store_factory)
      .Run(syncer::WEB_APPS,
           base::BindOnce(&WebAppDatabase::OnDatabaseOpened,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppDatabase::Write(
    const RegistryUpdateData& update_data,
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(opened_);

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  // |update_data| can be empty here but we should write |metadata_change_list|
  // anyway.
  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_create) {
    auto proto = CreateWebAppProto(*web_app);
    write_batch->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_update) {
    auto proto = CreateWebAppProto(*web_app);
    write_batch->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  for (const AppId& app_id : update_data.apps_to_delete)
    write_batch->DeleteData(app_id);

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&WebAppDatabase::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// static
std::unique_ptr<WebAppProto> WebAppDatabase::CreateWebAppProto(
    const WebApp& web_app) {
  auto local_data = std::make_unique<WebAppProto>();

  // Required fields:
  const GURL launch_url = web_app.launch_url();
  DCHECK(!launch_url.is_empty() && launch_url.is_valid());

  DCHECK(!web_app.app_id().empty());
  DCHECK_EQ(web_app.app_id(), GenerateAppIdFromURL(launch_url));

  sync_pb::WebAppSpecifics* sync_data = local_data->mutable_sync_data();

  sync_data->set_launch_url(launch_url.spec());

  local_data->set_name(web_app.name());

  sync_data->set_user_display_mode(
      ToWebAppSpecificsUserDisplayMode(web_app.user_display_mode()));

  DCHECK(web_app.sources_.any());
  local_data->mutable_sources()->set_system(web_app.sources_[Source::kSystem]);
  local_data->mutable_sources()->set_policy(web_app.sources_[Source::kPolicy]);
  local_data->mutable_sources()->set_web_app_store(
      web_app.sources_[Source::kWebAppStore]);
  local_data->mutable_sources()->set_sync(web_app.sources_[Source::kSync]);
  local_data->mutable_sources()->set_default_(
      web_app.sources_[Source::kDefault]);

  local_data->set_is_locally_installed(web_app.is_locally_installed());

  // Optional fields:
  if (web_app.display_mode() != DisplayMode::kUndefined) {
    local_data->set_display_mode(
        ToWebAppProtoDisplayMode(web_app.display_mode()));
  }
  local_data->set_description(web_app.description());
  if (!web_app.scope().is_empty())
    local_data->set_scope(web_app.scope().spec());
  if (web_app.theme_color().has_value())
    local_data->set_theme_color(web_app.theme_color().value());

  if (web_app.chromeos_data().has_value()) {
    auto& chromeos_data = web_app.chromeos_data().value();
    auto* mutable_chromeos_data = local_data->mutable_chromeos_data();
    mutable_chromeos_data->set_show_in_launcher(chromeos_data.show_in_launcher);
    mutable_chromeos_data->set_show_in_search(chromeos_data.show_in_search);
    mutable_chromeos_data->set_show_in_management(
        chromeos_data.show_in_management);
    mutable_chromeos_data->set_is_disabled(chromeos_data.is_disabled);
  }

  local_data->set_is_in_sync_install(web_app.is_in_sync_install());

  // Set sync_data to sync proto.
  sync_data->set_name(web_app.sync_data().name);
  if (web_app.sync_data().theme_color.has_value())
    sync_data->set_theme_color(web_app.sync_data().theme_color.value());

  for (const WebApplicationIconInfo& icon_info : web_app.icon_infos()) {
    WebAppIconInfoProto* icon_info_proto = local_data->add_icon_infos();
    icon_info_proto->set_size_in_px(icon_info.square_size_px);
    DCHECK(!icon_info.url.is_empty());
    icon_info_proto->set_url(icon_info.url.spec());
  }

  for (SquareSizePx size : web_app.downloaded_icon_sizes())
    local_data->add_downloaded_icon_sizes(size);

  for (const auto& file_handler : web_app.file_handlers()) {
    WebAppFileHandlerProto* file_handler_proto =
        local_data->add_file_handlers();
    file_handler_proto->set_action(file_handler.action.spec());

    for (const auto& accept_entry : file_handler.accept) {
      WebAppFileHandlerAcceptProto* accept_entry_proto =
          file_handler_proto->add_accept();
      accept_entry_proto->set_mimetype(accept_entry.mime_type);

      for (const auto& file_extension : accept_entry.file_extensions)
        accept_entry_proto->add_file_extensions(file_extension);
    }
  }

  for (const auto& additional_search_term : web_app.additional_search_terms()) {
    // Additional search terms should be sanitized before being added here.
    DCHECK(!additional_search_term.empty());
    local_data->add_additional_search_terms(additional_search_term);
  }

  return local_data;
}

// static
std::unique_ptr<WebApp> WebAppDatabase::CreateWebApp(
    const WebAppProto& local_data) {
  if (!local_data.has_sync_data()) {
    DLOG(ERROR) << "WebApp proto parse error: no sync_data field";
    return nullptr;
  }

  const sync_pb::WebAppSpecifics& sync_data = local_data.sync_data();

  // AppId is a hash of launch_url. Read launch_url first:
  GURL launch_url(sync_data.launch_url());
  if (launch_url.is_empty() || !launch_url.is_valid()) {
    DLOG(ERROR) << "WebApp proto launch_url parse error: "
                << launch_url.possibly_invalid_spec();
    return nullptr;
  }

  const AppId app_id = GenerateAppIdFromURL(launch_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetLaunchUrl(launch_url);

  // Required fields:
  if (!local_data.has_sources()) {
    DLOG(ERROR) << "WebApp proto parse error: no sources field";
    return nullptr;
  }

  WebApp::Sources sources;
  sources[Source::kSystem] = local_data.sources().system();
  sources[Source::kPolicy] = local_data.sources().policy();
  sources[Source::kWebAppStore] = local_data.sources().web_app_store();
  sources[Source::kSync] = local_data.sources().sync();
  sources[Source::kDefault] = local_data.sources().default_();
  if (!sources.any()) {
    DLOG(ERROR) << "WebApp proto parse error: no any source in sources field";
    return nullptr;
  }
  web_app->sources_ = sources;

  if (!local_data.has_name()) {
    DLOG(ERROR) << "WebApp proto parse error: no name field";
    return nullptr;
  }
  web_app->SetName(local_data.name());

  if (!sync_data.has_user_display_mode()) {
    DLOG(ERROR) << "WebApp proto parse error: no user_display_mode field";
    return nullptr;
  }
  web_app->SetUserDisplayMode(
      ToMojomDisplayMode(sync_data.user_display_mode()));

  if (!local_data.has_is_locally_installed()) {
    DLOG(ERROR) << "WebApp proto parse error: no is_locally_installed field";
    return nullptr;
  }
  web_app->SetIsLocallyInstalled(local_data.is_locally_installed());

  auto& chromeos_data_proto = local_data.chromeos_data();

  if (IsChromeOs() && !local_data.has_chromeos_data()) {
    DLOG(ERROR) << "WebApp proto parse error: no chromeos_data field. The web "
                << "app might have been installed when running on an OS other "
                << "than Chrome OS.";
    return nullptr;
  }

  if (!IsChromeOs() && local_data.has_chromeos_data()) {
    DLOG(ERROR) << "WebApp proto parse error: has chromeos_data field. The web "
                << "app might have been installed when running on Chrome OS.";
    return nullptr;
  }

  if (local_data.has_chromeos_data()) {
    auto chromeos_data = base::make_optional<WebAppChromeOsData>();
    chromeos_data->show_in_launcher = chromeos_data_proto.show_in_launcher();
    chromeos_data->show_in_search = chromeos_data_proto.show_in_search();
    chromeos_data->show_in_management =
        chromeos_data_proto.show_in_management();
    chromeos_data->is_disabled = chromeos_data_proto.is_disabled();
    web_app->SetWebAppChromeOsData(std::move(chromeos_data));
  }

  // Optional fields:
  if (local_data.has_display_mode())
    web_app->SetDisplayMode(ToMojomDisplayMode(local_data.display_mode()));

  if (local_data.has_description())
    web_app->SetDescription(local_data.description());

  if (local_data.has_scope()) {
    GURL scope(local_data.scope());
    if (scope.is_empty() || !scope.is_valid()) {
      DLOG(ERROR) << "WebApp proto scope parse error: "
                  << scope.possibly_invalid_spec();
      return nullptr;
    }
    web_app->SetScope(scope);
  }

  if (local_data.has_theme_color())
    web_app->SetThemeColor(local_data.theme_color());

  if (local_data.has_is_in_sync_install())
    web_app->SetIsInSyncInstall(local_data.is_in_sync_install());

  // Parse sync_data from sync proto.
  WebApp::SyncData parsed_sync_data;
  if (sync_data.has_name())
    parsed_sync_data.name = sync_data.name();
  if (sync_data.has_theme_color())
    parsed_sync_data.theme_color = sync_data.theme_color();
  web_app->SetSyncData(std::move(parsed_sync_data));

  std::vector<WebApplicationIconInfo> icon_infos;
  for (const WebAppIconInfoProto& icon_info_proto : local_data.icon_infos()) {
    WebApplicationIconInfo icon_info;
    icon_info.square_size_px = icon_info_proto.size_in_px();
    if (!icon_info_proto.has_url()) {
      DLOG(ERROR) << "WebApp IconInfo has missing url";
      return nullptr;
    }
    GURL icon_url(icon_info_proto.url());
    if (icon_url.is_empty() || !icon_url.is_valid()) {
      DLOG(ERROR) << "WebApp IconInfo proto url parse error: "
                  << icon_url.possibly_invalid_spec();
      return nullptr;
    }
    icon_info.url = icon_url;

    icon_infos.push_back(std::move(icon_info));
  }
  web_app->SetIconInfos(std::move(icon_infos));

  std::vector<SquareSizePx> icon_sizes_on_disk;
  for (int32_t size : local_data.downloaded_icon_sizes())
    icon_sizes_on_disk.push_back(size);
  web_app->SetDownloadedIconSizes(std::move(icon_sizes_on_disk));

  apps::FileHandlers file_handlers;
  for (const auto& file_handler_proto : local_data.file_handlers()) {
    apps::FileHandler file_handler;
    file_handler.action = GURL(file_handler_proto.action());

    if (file_handler.action.is_empty() || !file_handler.action.is_valid()) {
      DLOG(ERROR) << "WebApp FileHandler proto action parse error";
      return nullptr;
    }

    for (const auto& accept_entry_proto : file_handler_proto.accept()) {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = accept_entry_proto.mimetype();
      for (const auto& file_extension : accept_entry_proto.file_extensions()) {
        if (base::Contains(accept_entry.file_extensions, file_extension)) {
          // We intentionally don't return a nullptr here; instead, duplicate
          // entries are absorbed.
          DLOG(ERROR) << "apps::FileHandler::AcceptEntry parsing encountered "
                      << "duplicate file extension";
        }
        accept_entry.file_extensions.insert(file_extension);
      }
      file_handler.accept.push_back(std::move(accept_entry));
    }

    file_handlers.push_back(std::move(file_handler));
  }
  web_app->SetFileHandlers(std::move(file_handlers));

  std::vector<std::string> additional_search_terms;
  for (const std::string& additional_search_term :
       local_data.additional_search_terms()) {
    if (additional_search_term.empty()) {
      DLOG(ERROR) << "WebApp AdditionalSearchTerms proto action parse error";
      return nullptr;
    }
    additional_search_terms.push_back(additional_search_term);
  }
  web_app->SetAdditionalSearchTerms(std::move(additional_search_terms));

  return web_app;
}

void WebAppDatabase::OnDatabaseOpened(
    RegistryOpenedCallback callback,
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB opening error: " << error->ToString();
    return;
  }

  store_ = std::move(store);
  // TODO(loyso): Use ReadAllDataAndPreprocess to parse protos in the background
  // sequence.
  store_->ReadAllData(base::BindOnce(&WebAppDatabase::OnAllDataRead,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

void WebAppDatabase::OnAllDataRead(
    RegistryOpenedCallback callback,
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB data read error: " << error->ToString();
    return;
  }

  store_->ReadAllMetadata(base::BindOnce(
      &WebAppDatabase::OnAllMetadataRead, weak_ptr_factory_.GetWeakPtr(),
      std::move(data_records), std::move(callback)));
}

void WebAppDatabase::OnAllMetadataRead(
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records,
    RegistryOpenedCallback callback,
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB metadata read error: " << error->ToString();
    return;
  }

  Registry registry;
  for (const syncer::ModelTypeStore::Record& record : *data_records) {
    const AppId app_id = record.id;
    std::unique_ptr<WebApp> web_app = ParseWebApp(app_id, record.value);
    if (web_app)
      registry.emplace(app_id, std::move(web_app));
  }

  std::move(callback).Run(std::move(registry), std::move(metadata_batch));
  opened_ = true;
}

void WebAppDatabase::OnDataWritten(
    CompletionCallback callback,
    const base::Optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB write error: " << error->ToString();
  }

  std::move(callback).Run(!error);
}

// static
std::unique_ptr<WebApp> WebAppDatabase::ParseWebApp(const AppId& app_id,
                                                    const std::string& value) {
  WebAppProto proto;
  const bool parsed = proto.ParseFromString(value);
  if (!parsed) {
    DLOG(ERROR) << "WebApps LevelDB parse error: can't parse proto.";
    return nullptr;
  }

  auto web_app = CreateWebApp(proto);
  if (web_app->app_id() != app_id) {
    DLOG(ERROR) << "WebApps LevelDB error: app_id doesn't match storage key";
    return nullptr;
  }

  return web_app;
}

DisplayMode ToMojomDisplayMode(WebAppProto::DisplayMode display_mode) {
  switch (display_mode) {
    case WebAppProto::BROWSER:
      return DisplayMode::kBrowser;
    case WebAppProto::MINIMAL_UI:
      return DisplayMode::kMinimalUi;
    case WebAppProto::STANDALONE:
      return DisplayMode::kStandalone;
    case WebAppProto::FULLSCREEN:
      return DisplayMode::kFullscreen;
  }
}

DisplayMode ToMojomDisplayMode(
    ::sync_pb::WebAppSpecifics::UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case ::sync_pb::WebAppSpecifics::BROWSER:
      return DisplayMode::kBrowser;
    case ::sync_pb::WebAppSpecifics::STANDALONE:
      return DisplayMode::kStandalone;
  }
}

WebAppProto::DisplayMode ToWebAppProtoDisplayMode(DisplayMode display_mode) {
  switch (display_mode) {
    case DisplayMode::kBrowser:
      return WebAppProto::BROWSER;
    case DisplayMode::kMinimalUi:
      return WebAppProto::MINIMAL_UI;
    case DisplayMode::kUndefined:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
      return WebAppProto::STANDALONE;
    case DisplayMode::kFullscreen:
      return WebAppProto::FULLSCREEN;
  }
}

::sync_pb::WebAppSpecifics::UserDisplayMode ToWebAppSpecificsUserDisplayMode(
    DisplayMode user_display_mode) {
  switch (user_display_mode) {
    case DisplayMode::kBrowser:
      return ::sync_pb::WebAppSpecifics::BROWSER;
    case DisplayMode::kUndefined:
    case DisplayMode::kMinimalUi:
    case DisplayMode::kFullscreen:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
      return ::sync_pb::WebAppSpecifics::STANDALONE;
  }
}

}  // namespace web_app
