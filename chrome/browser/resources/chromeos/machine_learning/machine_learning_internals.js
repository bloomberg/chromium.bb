// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ModelId = chromeos.machineLearning.mojom.ModelId;
const LoadModelResult = chromeos.machineLearning.mojom.LoadModelResult;
const CreateGraphExecutorResult =
    chromeos.machineLearning.mojom.CreateGraphExecutorResult;
const ExecuteResult = chromeos.machineLearning.mojom.ExecuteResult;

cr.ui.decorate('tabbox', cr.ui.TabBox);

cr.define('machine_learning_internals', function() {
  class BrowserProxy {
    constructor() {
      /**
       * @type {!chromeos.machineLearning.mojom.PageHandlerProxy}
       */
      this.pageHandler = chromeos.machineLearning.mojom.PageHandler.getProxy();
      /**
       * @private {!Map<ModelId,
       *     !chromeos.machineLearning.mojom.GraphExecutorProxy>}
       */
      this.modelMap_ = new Map();
    }

    /**
     * @param {!ModelId} modelId Model to load.
     * @return
     * {!Promise<!chromeos.machineLearning.mojom.GraphExecutorProxy>} A
     *     promise that resolves when loadModel and createGraphExecutor both
     *     succeed.
     */
    async prepareModel(modelId) {
      if (this.modelMap_.has(modelId)) {
        return this.modelMap_.get(modelId);
      }
      const modelSpec = {id: modelId};
      /** @type {chromeos.machineLearning.mojom.ModelProxy} */
      const model = new chromeos.machineLearning.mojom.ModelProxy();
      const loadModelResponse =
          await this.pageHandler.loadModel(modelSpec, model.$.createRequest());
      if (loadModelResponse.result != LoadModelResult.OK) {
        const error = machine_learning_internals.utils.enumToString(
            loadModelResponse.result, LoadModelResult);
        throw new Error(`LoadModel failed! Error: ${error}.`);
      }
      /** @type {chromeos.machineLearning.mojom.GraphExecutorProxy} */
      const graphExecutorProxy =
          new chromeos.machineLearning.mojom.GraphExecutorProxy();
      const createGraphExecutorResponse =
          await model.createGraphExecutor(graphExecutorProxy.$.createRequest());
      if (createGraphExecutorResponse.result != CreateGraphExecutorResult.OK) {
        const error = machine_learning_internals.utils.enumToString(
            createGraphExecutorResponse.result, CreateGraphExecutorResult);
        throw new Error(`CreateGraphExecutor failed! Error: ${error}.`);
      }

      this.modelMap_.set(modelId, graphExecutorProxy);
      return graphExecutorProxy;
    }
  }

  cr.addSingletonGetter(BrowserProxy);

  return {BrowserProxy: BrowserProxy};
});
