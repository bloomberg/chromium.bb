// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_ENTITY_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_ENTITY_H_

#include <stdint.h>

#include <map>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"

namespace fake_server {

// The representation of a Sync entity for the fake server.
class FakeServerEntity {
 public:
  // Creates an ID of the form <type><separator><inner-id> where
  // <type> is the EntitySpecifics field number for |model_type|, <separator>
  // is kIdSeparator, and <inner-id> is |inner_id|.
  //
  // If |inner_id| is globally unique, then the returned ID will also be
  // globally unique.
  static std::string CreateId(const syncer::ModelType& model_type,
                              const std::string& inner_id);

  // Returns the ID string of the top level node for the specified type.
  static std::string GetTopLevelId(const syncer::ModelType& model_type);

  virtual ~FakeServerEntity();
  const std::string& id() const { return id_; }
  const std::string& client_defined_unique_tag() const {
    return client_defined_unique_tag_;
  }
  syncer::ModelType model_type() const { return model_type_; }
  int64_t GetVersion() const;
  void SetVersion(int64_t version);
  const std::string& GetName() const;
  void SetName(const std::string& name);

  // Replaces |specifics_| with |updated_specifics|. This method is meant to be
  // used to mimic a client commit.
  void SetSpecifics(const sync_pb::EntitySpecifics& updated_specifics);

  // Common data items needed by server
  virtual bool RequiresParentId() const = 0;
  virtual std::string GetParentId() const = 0;
  virtual void SerializeAsProto(sync_pb::SyncEntity* proto) const = 0;
  virtual bool IsDeleted() const;
  virtual bool IsFolder() const;
  virtual bool IsPermanent() const;

 protected:
  // Extracts the ModelType from |id|. If |id| is malformed or does not contain
  // a valid ModelType, UNSPECIFIED is returned.
  static syncer::ModelType GetModelTypeFromId(const std::string& id);

  FakeServerEntity(const std::string& id,
                   const std::string& client_defined_unique_tag,
                   const syncer::ModelType& model_type,
                   int64_t version,
                   const std::string& name);

  void SerializeBaseProtoFields(sync_pb::SyncEntity* sync_entity) const;

 private:
  // The entity's ID.
  const std::string id_;

  // The tag for this entity. Can be empty for bookmarks or permanent entities.
  const std::string client_defined_unique_tag_;

  // The ModelType that categorizes this entity.
  const syncer::ModelType model_type_;

  // The version of this entity.
  int64_t version_;

  // The name of the entity.
  std::string name_;

  // The EntitySpecifics for the entity.
  sync_pb::EntitySpecifics specifics_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_ENTITY_H_
