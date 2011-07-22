// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/composite_data_source_factory.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"

namespace media {

typedef std::list<DataSourceFactory*> FactoryList;

static void CallNextFactory(FactoryList* factories,
                            const std::string& url,
                            const DataSourceFactory::BuildCB& callback);

// Called when the first factory in |factories| completes a Build() request.
// |factories| - The list of factories to try. Ownership is being
//               passed to this function here.
// |url| - The URL from the originating Build() call.
// |callback| - The callback from the originating Build() call.
// |status| - The status returned from the factory.
// |data_source| - The DataSource built by the factory. NULL if
//                 |status| is not PIPELINE_OK. Ownership is passed here.
static void OnBuildDone(FactoryList* factories,
                        const std::string& url,
                        const DataSourceFactory::BuildCB& callback,
                        PipelineStatus status,
                        DataSource* data_source) {
  DCHECK(factories);
  DCHECK(!factories->empty());

  // Remove & destroy front factory since we are done using it now.
  delete factories->front();
  factories->pop_front();

  if (status == PIPELINE_OK) {
    DCHECK(data_source);
    callback.Run(status, data_source);

    // Delete the factory list and all remaining factories.
    STLDeleteElements(factories);
    delete factories;
    return;
  }

  // Try the next factory if |status| indicates the factory didn't know how to
  // build a DataSource for |url|.
  DCHECK(!data_source);
  if ((status == DATASOURCE_ERROR_URL_NOT_SUPPORTED) &&
      !factories->empty()) {
    CallNextFactory(factories, url, callback);
    return;
  }

  // No more factories to try or a factory that could handle the
  // request encountered an error.
  callback.Run(status, NULL);

  // Delete the factory list and all remaining factories.
  STLDeleteElements(factories);
  delete factories;
}

// Calls Build() on the front factory in |factories|.
// Ownership of |factories| is passed to this method.
static void CallNextFactory(FactoryList* factories,
                            const std::string& url,
                            const DataSourceFactory::BuildCB& callback) {
  DCHECK(factories);
  DCHECK(!factories->empty());

  DataSourceFactory* factory = factories->front();
  factory->Build(url, base::Bind(&OnBuildDone,
                                 factories,
                                 url,
                                 callback));
}

CompositeDataSourceFactory::CompositeDataSourceFactory() {}

CompositeDataSourceFactory::~CompositeDataSourceFactory() {
  STLDeleteElements(&factories_);
}

void CompositeDataSourceFactory::AddFactory(DataSourceFactory* factory) {
  DCHECK(factory);
  factories_.push_back(factory);
}

void CompositeDataSourceFactory::Build(const std::string& url,
                                       const BuildCB& callback) {
  if (factories_.empty()) {
    callback.Run(DATASOURCE_ERROR_URL_NOT_SUPPORTED, NULL);
    return;
  }

  // Construct the list of factories to try.
  scoped_ptr<FactoryList> factories(new FactoryList());
  for (FactoryList::const_iterator itr = factories_.begin();
       itr != factories_.end();
       ++itr) {
    factories->push_back((*itr)->Clone());
  }

  // Start trying to build a DataSource from the factories in the list.
  CallNextFactory(factories.release(), url, callback);
}

DataSourceFactory* CompositeDataSourceFactory::Clone() const {
  CompositeDataSourceFactory* new_factory = new CompositeDataSourceFactory();

  for (FactoryList::const_iterator itr = factories_.begin();
       itr != factories_.end();
       ++itr) {
    new_factory->AddFactory((*itr)->Clone());
  }

  return new_factory;
}

}  // namespace media
