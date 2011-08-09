// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('tracing', function() {
  var dataSets = [
    {
      name: 'big_trace',
      events_url: './tests/big_trace.json'
    },
    {
      name: 'trivial_trace',
      events: [
        {'name':'a','args':{},'pid':52,'ts':9524,'cat':'foo','tid':53,'ph':'B'},
        {'name':'a','args':{},'pid':52,'ts':9560,'cat':'foo','tid':53,'ph':'E'},
        {'name':'b','args':{},'pid':52,'ts':9629,'cat':'foo','tid':53,'ph':'B'},
        {'name':'b','args':{},'pid':52,'ts':9631,'cat':'foo','tid':53,'ph':'E'}
      ]
    },
    {
      name: 'simple_trace',
      events: [
        {'cat':'PERF','pid':22630,'tid':22630,'ts':826,'ph':'B',
         'name':'A long name that doesn't fit but is exceedingly informative',
         'args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':827,'ph':'B',
         'name':'Asub with a name that won't fit','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':828,'ph':'E',
         'name':'Asub','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':829,'ph':'B',
         'name':'Asub','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':832,'ph':'E',
         'name':'Asub','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':833,'ph':'E',
         'name':'','args':{}},

        {'cat':'PERF','pid':22630,'tid':22630,'ts':835,'ph':'I',
         'name':'I1','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':837,'ph':'I',
         'name':'I2','args':{}},

        {'cat':'PERF','pid':22630,'tid':22630,'ts':840,'ph':'B',
         'name':'A not as long a name','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':848,'ph':'E',
         'name':'A not as long a name','args':{}},

        {'cat':'PERF','pid':22630,'tid':22630,'ts':850,'ph':'B',
         'name':'B','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':854,'ph':'E',
         'name':'B','args':{}},

        {'cat':'PERF','pid':22630,'tid':22631,'ts':827,'ph':'B',
         'name':'A','args':{}},
        {'cat':'PERF','pid':22630,'tid':22631,'ts':835,'ph':'I',
         'name':'Immediate Three','args':{}},
        {'cat':'PERF','pid':22630,'tid':22631,'ts':845,'ph':'I',
         'name':'I4','args':{}},
        {'cat':'PERF','pid':22630,'tid':22631,'ts':854,'ph':'E',
         'name':'A','args':{}},

        {'cat':'__metadata','pid':22630,'tid':22630,'ts':0,'ph':'M',
         'name':'thread_name','args':{'name': 'threadA'}},
        {'cat':'__metadata','pid':22630,'tid':22631,'ts':0,'ph':'M',
         'name':'thread_name','args':{'name': 'threadB'}},
        {'cat':'__metadata','pid':22630,'tid':22632,'ts':0,'ph':'M',
         'name':'thread_name','args':{'name': 'threadC'}}
      ]
    },
    {
      name: 'nonnested_trace',
      events: [
        {'cat':'PERF','pid':22630,'tid':22630,'ts':826,'ph':'B',
         'name':'A','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':827,'ph':'B',
         'name':'Asub','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':829,'ph':'B',
         'name':'NonNest','args':{'id':'1','ui-nest':'0'}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':830,'ph':'B',
         'name':'NonNest','args':{'id':'2','ui-nest':'0'}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':831,'ph':'E',
         'name':'Asub','args':{}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':832,'ph':'E',
         'name':'NonNest','args':{'id':'1','ui-nest':'0'}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':833,'ph':'E',
         'name':'NonNest','args':{'id':'2','ui-nest':'0'}},
        {'cat':'PERF','pid':22630,'tid':22630,'ts':834,'ph':'E',
         'name':'A','args':{}},

        {'cat':'PERF','pid':22630,'tid':22631,'ts':827,'ph':'B',
         'name':'A','args':{}},
        {'cat':'PERF','pid':22630,'tid':22631,'ts':854,'ph':'E',
         'name':'A','args':{}}
      ]
    },
    {
      name: 'tall_trace',
      events: [
        {'cat':'X','pid':30,'tid':30,'ts':826,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':30,'ts':827,'ph':'B','name':'Asub','args':{}},
        {'cat':'X','pid':30,'tid':30,'ts':828,'ph':'E','name':'Asub','args':{}},
        {'cat':'X','pid':30,'tid':30,'ts':829,'ph':'B','name':'Asub','args':{}},
        {'cat':'X','pid':30,'tid':30,'ts':832,'ph':'E','name':'Asub','args':{}},
        {'cat':'X','pid':30,'tid':30,'ts':833,'ph':'E','name':'','args':{}},

        {'cat':'X','pid':30,'tid':31,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':31,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':32,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':32,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':33,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':33,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':34,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':34,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':35,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':35,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':36,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':36,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':37,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':37,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':38,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':38,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':39,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':39,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':10,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':10,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':31,'tid':11,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':31,'tid':11,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':12,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':12,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':13,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':13,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':14,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':14,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'X','pid':30,'tid':15,'ts':840,'ph':'B','name':'A','args':{}},
        {'cat':'X','pid':30,'tid':15,'ts':848,'ph':'E','name':'A','args':{}},

        {'cat':'__metadata','pid':30,'tid':14,'ts':0,'ph':'M',
         'name':'thread_name','args':{'name': 'threadB'}},
        {'cat':'__metadata','pid':30,'tid':15,'ts':0,'ph':'M',
         'name':'thread_name','args':{'name': 'threadA'}}
      ]
    },
    {
       name: 'huge_trace',
       events_url: './tests/huge_trace.json'
    }
  ];

  // Create UI for controlling the test harness
  var selectEl = document.createElement('select');
  for (var i = 0; i < dataSets.length; ++i) {
    var optionEl = document.createElement('option');
    optionEl.textContent = dataSets[i].name;
    optionEl.dataSet = dataSets[i];
    selectEl.appendChild(optionEl);
  }
  selectEl.addEventListener('change', function() {
    tracingController.beginTracing();
  });
  selectEl.addEventListener('keydown', function() {
    window.setTimeout(function() {
      tracingController.beginTracing();
    }, 0);
  });

  var controlEl = document.createElement('div');
  var textEl = document.createElement('span');
  textEl.textContent = 'Trace:';
  controlEl.appendChild(textEl);
  controlEl.appendChild(selectEl);

  document.querySelector('#debug-div').appendChild(controlEl,
                                                   document.body.firstChild);

  return {
    tracingControllerTestHarness : {
      beginTracing: function() {
        var dataSet = dataSets[selectEl.selectedIndex];
        if (dataSet.events) {
          window.setTimeout(function() {
            tracingController.onTraceDataCollected(dataSet.events);
            tracingController.endTracing();
            window.setTimeout(function() {
              tracingController.onEndTracingComplete();
            },0);
          }, 0);
        } else {
          var req = new XMLHttpRequest();
          req.open('GET', './tracing/' + dataSet.events_url, true);
          req.onreadystatechange = function (aEvt) {
            if (req.readyState == 4) {
              tracingController.endTracing();
              window.setTimeout(function() {
                if(req.status == 200) {
                  var resp = JSON.parse(req.responseText);
                  if (resp.traceEvents)
                    tracingController.onTraceDataCollected(resp.traceEvents);
                  else
                    tracingController.onTraceDataCollected(resp);
                } else {
                  console.log('collection failed.');
                }
                tracingController.onEndTracingComplete();
              }, 0);
            }
          };
          req.send(null);
        }
      },

      endTracing: function() {
      }
    }
  };
});
