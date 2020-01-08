function perfTestAddToLog(text) {
  parent.postMessage({'command': 'log', 'value': text}, '*');
}

function perfTestAddToSummary(text) {
}

function perfTestMeasureValue(value) {
  parent.postMessage({'command': 'measureValue', 'value': value}, '*');
}

function perfTestNotifyAbort() {
  parent.postMessage({'command': 'notifyAbort'}, '*');
}

function getConfigForPerformanceTest(dataType, async,
                                     verifyData, numIterations,
                                     numWarmUpIterations) {

  return {
    prefixUrl: 'ws://' + location.host + '/benchmark_helper',
    printSize: true,
    numSockets: 1,
    // + 1 is for a warmup iteration by the Telemetry framework.
    numIterations: numIterations + numWarmUpIterations + 1,
    numWarmUpIterations: numWarmUpIterations,
    minTotal: 10240000,
    startSize: 10240000,
    stopThreshold: 10240000,
    multipliers: [2],
    verifyData: verifyData,
    dataType: dataType,
    async: async,
    addToLog: perfTestAddToLog,
    addToSummary: perfTestAddToSummary,
    measureValue: perfTestMeasureValue,
    notifyAbort: perfTestNotifyAbort
  };
}

var data;
onmessage = function(message) {
  var action;
  if (message.data.command === 'start') {
    data = message.data;
    initWorker('http://' + location.host);
    action = data.benchmarkName;
  } else {
    action = 'stop';
  }

  var config = getConfigForPerformanceTest(data.dataType, data.async,
                                           data.verifyData,
                                           data.numIterations,
                                           data.numWarmUpIterations);
  doAction(config, data.isWorker, action);
};
