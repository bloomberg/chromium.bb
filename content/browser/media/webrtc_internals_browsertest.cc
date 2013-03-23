// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"

using std::string;
namespace content {

struct EventEntry {
  string type;
  string value;
};

struct StatsUnit {
  string GetString() const {
    std::stringstream ss;
    ss << "{timestamp:" << timestamp << ", values:[";
    std::map<string, string>::const_iterator iter;
    for (iter = values.begin(); iter != values.end(); iter++) {
      ss << "'" << iter->first << "','" << iter->second << "',";
    }
    ss << "]}";
    return ss.str();
  }

  int64 timestamp;
  std::map<string, string> values;
};

struct StatsEntry {
  string type;
  string id;
  StatsUnit local;
  StatsUnit remote;
};

typedef std::map<string, std::vector<string> > StatsMap;

class PeerConnectionEntry {
 public:
  PeerConnectionEntry(int pid, int lid) : pid_(pid), lid_(lid) {}

  void AddEvent(const string& type, const string& value) {
    EventEntry entry = {type, value};
    events_.push_back(entry);
  }

  string getIdString() const {
    std::stringstream ss;
    ss << pid_ << "-" << lid_;
    return ss.str();
  }

  string getLogIdString() const {
    std::stringstream ss;
    ss << pid_ << "-" << lid_ << "-log";
    return ss.str();
  }

  string getAllUpdateString() const {
    std::stringstream ss;
    ss << "{pid:" << pid_ << ", lid:" << lid_ << ", log:[";
    for (size_t i = 0; i < events_.size(); ++i) {
      ss << "{type:'" << events_[i].type <<
          "', value:'" << events_[i].value << "'},";
    }
    ss << "]}";
    return ss.str();
  }

  int pid_;
  int lid_;
  std::vector<EventEntry> events_;
  // This is a record of the history of stats value reported for each stats
  // report id (e.g. ssrc-1234) for each stats name (e.g. framerate).
  // It a 2-D map with each map entry is a vector of reported values.
  // It is used to verify the graph data series.
  std::map<string, StatsMap> stats_;
};

static const int64 FAKE_TIME_STAMP = 0;

class WebRTCInternalsBrowserTest: public ContentBrowserTest {
 public:
  WebRTCInternalsBrowserTest() {}
  virtual ~WebRTCInternalsBrowserTest() {}

 protected:
  bool ExecuteJavascript(const string& javascript) {
    return ExecuteScript(shell()->web_contents(), javascript);
  }

  // Execute the javascript of addPeerConnection.
  void ExecuteAddPeerConnectionJs(const PeerConnectionEntry& pc) {
    std::stringstream ss;
    ss << "{pid:" << pc.pid_ <<", lid:" << pc.lid_ << ", " <<
           "url:'u', servers:'s', constraints:'c'}";
    ASSERT_TRUE(ExecuteJavascript("addPeerConnection(" + ss.str() + ");"));
  }

  // Execute the javascript of removePeerConnection.
  void ExecuteRemovePeerConnectionJs(const PeerConnectionEntry& pc) {
    std::stringstream ss;
    ss << "{pid:" << pc.pid_ <<", lid:" << pc.lid_ << "}";

    ASSERT_TRUE(ExecuteJavascript("removePeerConnection(" + ss.str() + ");"));
  }

  // Verifies that the DOM element with id |id| exists.
  void VerifyElementWithId(const string& id) {
    bool result = false;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell()->web_contents(),
        "window.domAutomationController.send($('" + id + "') != null);",
        &result));
    EXPECT_TRUE(result);
  }

  // Verifies that the DOM element with id |id| does not exist.
  void VerifyNoElementWithId(const string& id) {
    bool result = false;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell()->web_contents(),
        "window.domAutomationController.send($('" + id + "') == null);",
        &result));
    EXPECT_TRUE(result);
  }

  // Verifies that DOM for |pc| is correctly created with the right content.
  void VerifyPeerConnectionEntry(const PeerConnectionEntry& pc) {
    VerifyElementWithId(pc.getIdString());
    if (pc.events_.size() == 0)
      return;

    string log_id = pc.getLogIdString();
    VerifyElementWithId(log_id);
    string result;
    for (size_t i = 0; i < pc.events_.size(); ++i) {
      std::stringstream ss;
      ss << "var row = $('" << log_id << "').rows[" << (i + 1) << "];"
            "var cell = row.lastChild;"
            "var type = cell.firstChild.textContent;"
            "var value = cell.lastChild.lastChild.value;"
            "window.domAutomationController.send(type + ':' + value);";
      ASSERT_TRUE(ExecuteScriptAndExtractString(
          shell()->web_contents(), ss.str(), &result));
      EXPECT_EQ(pc.events_[i].type + ":" + pc.events_[i].value, result);
    }
  }

  // Executes the javascript of updatePeerConnection and verifies the result.
  void ExecuteAndVerifyUpdatePeerConnection(
      PeerConnectionEntry& pc, const string& type, const string& value) {
    pc.AddEvent(type, value);

    std::stringstream ss;
    ss << "{pid:" << pc.pid_ <<", lid:" << pc.lid_ <<
         ", type:'" << type << "', value:'" << value << "'}";
    ASSERT_TRUE(ExecuteJavascript("updatePeerConnection(" + ss.str() + ")"));

    VerifyPeerConnectionEntry(pc);
  }

  // Execute addStats and verifies that the stats table has the right content.
  void ExecuteAndVerifyAddStats(
      PeerConnectionEntry& pc, const string& type, const string& id,
      StatsUnit& local, StatsUnit& remote) {
    StatsEntry entry = {type, id, local, remote};

    // Adds each new value to the map of stats history.
    std::map<string, string>::iterator iter;
    for (iter = local.values.begin(); iter != local.values.end(); iter++) {
      pc.stats_[type + "-" + id][iter->first].push_back(iter->second);
    }
    for (iter = remote.values.begin(); iter != remote.values.end(); iter++) {
      pc.stats_[type + "-" + id][iter->first].push_back(iter->second);
    }

    std::stringstream ss;
    ss << "{pid:" << pc.pid_ << ", lid:" << pc.lid_ << ","
           "reports:[" << "{id:'" << id << "', type:'" << type << "', "
                           "local:" << local.GetString() << ", "
                           "remote:" << remote.GetString() << "}]}";

    ASSERT_TRUE(ExecuteJavascript("addStats(" + ss.str() + ")"));
    VerifyStatsTable(pc, entry);
  }


  // Verifies that the stats table has the right content.
  void VerifyStatsTable(const PeerConnectionEntry& pc,
                       const StatsEntry& report) {
    string table_id =
        pc.getIdString() + "-table-" + report.type + "-" + report.id;
    VerifyElementWithId(table_id);

    std::map<string, string>::const_iterator iter;
    for (iter = report.local.values.begin();
         iter != report.local.values.end(); iter++) {
      VerifyStatsTableRow(table_id, iter->first, iter->second);
    }
    for (iter = report.remote.values.begin();
         iter != report.remote.values.end(); iter++) {
      VerifyStatsTableRow(table_id, iter->first, iter->second);
    }
  }

  // Verifies that the row named as |name| of the stats table |table_id| has
  // the correct content as |name| : |value|.
  void VerifyStatsTableRow(const string& table_id,
                           const string& name,
                           const string& value) {
    VerifyElementWithId(table_id + "-" + name);

    string result;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        shell()->web_contents(),
        "var row = $('" + table_id + "-" + name + "');"
        "var name = row.cells[0].textContent;"
        "var value = row.cells[1].textContent;"
        "window.domAutomationController.send(name + ':' + value)",
        &result));
    EXPECT_EQ(name + ":" + value, result);
  }

  // Verifies that the graph data series consistent with pc.stats_.
  void VerifyStatsGraph(const PeerConnectionEntry& pc) {
    std::map<string, StatsMap>::const_iterator stream_iter;
    for (stream_iter = pc.stats_.begin();
         stream_iter != pc.stats_.end(); stream_iter++) {
      StatsMap::const_iterator stats_iter;
      for (stats_iter = stream_iter->second.begin();
           stats_iter != stream_iter->second.end();
           stats_iter++) {
        for (size_t i = 0; i < stats_iter->second.size(); ++i) {
          string graph_id = pc.getIdString() + "-" +
              stream_iter->first + "-" + stats_iter->first;
          VerifyGraphDataPoint(graph_id, i, stats_iter->second[i]);
        }
      }
    }
  }

  // Verifies that the graph data point at index |index| has value |value|.
  void VerifyGraphDataPoint(
      const string& graph_id, int index, const string& value) {
    bool result = false;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell()->web_contents(),
        "window.domAutomationController.send("
        "   graphViews['" + graph_id + "'] != null)",
        &result));
    EXPECT_TRUE(result);

    std::stringstream ss;
    ss << "var dp = dataSeries['" << graph_id << "']"
              ".dataPoints_[" << index << "];"
          "window.domAutomationController.send(dp.value.toString())";
    string actual_value;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        shell()->web_contents(), ss.str(), &actual_value));
    EXPECT_EQ(value, actual_value);
  }
};

IN_PROC_BROWSER_TEST_F(WebRTCInternalsBrowserTest, AddAndRemovePeerConnection) {
  GURL url("chrome://webrtc-internals");
  NavigateToURL(shell(), url);

  // Add two PeerConnections and then remove them.
  PeerConnectionEntry pc_1(1, 0);
  ExecuteAddPeerConnectionJs(pc_1);
  VerifyPeerConnectionEntry(pc_1);

  PeerConnectionEntry pc_2(2, 1);
  ExecuteAddPeerConnectionJs(pc_2);
  VerifyPeerConnectionEntry(pc_2);

  ExecuteRemovePeerConnectionJs(pc_1);
  VerifyNoElementWithId(pc_1.getIdString());
  VerifyPeerConnectionEntry(pc_2);

  ExecuteRemovePeerConnectionJs(pc_2);
  VerifyNoElementWithId(pc_2.getIdString());
}

IN_PROC_BROWSER_TEST_F(WebRTCInternalsBrowserTest, UpdateAllPeerConnections) {
  GURL url("chrome://webrtc-internals");
  NavigateToURL(shell(), url);

  PeerConnectionEntry pc_0(1, 0);
  pc_0.AddEvent("e1", "v1");
  pc_0.AddEvent("e2", "v2");
  PeerConnectionEntry pc_1(1, 1);
  pc_1.AddEvent("e3", "v3");
  pc_1.AddEvent("e4", "v4");
  string pc_array = "[" + pc_0.getAllUpdateString() + ", " +
                          pc_1.getAllUpdateString() + "]";
  EXPECT_TRUE(ExecuteJavascript("updateAllPeerConnections(" + pc_array + ");"));
  VerifyPeerConnectionEntry(pc_0);
  VerifyPeerConnectionEntry(pc_1);
}

IN_PROC_BROWSER_TEST_F(WebRTCInternalsBrowserTest, UpdatePeerConnection) {
  GURL url("chrome://webrtc-internals");
  NavigateToURL(shell(), url);

  // Add one PeerConnection and send one update.
  PeerConnectionEntry pc_1(1, 0);
  ExecuteAddPeerConnectionJs(pc_1);

  ExecuteAndVerifyUpdatePeerConnection(pc_1, "e1", "v1");

  // Add another PeerConnection and send two updates.
  PeerConnectionEntry pc_2(1, 1);
  ExecuteAddPeerConnectionJs(pc_2);

  ExecuteAndVerifyUpdatePeerConnection(pc_2, "e2", "v2");
  ExecuteAndVerifyUpdatePeerConnection(pc_2, "e3", "v3");
}

// Tests that adding random named stats updates the dataSeries and graphs.
IN_PROC_BROWSER_TEST_F(WebRTCInternalsBrowserTest, AddStats) {
  GURL url("chrome://webrtc-internals");
  NavigateToURL(shell(), url);

  PeerConnectionEntry pc(1, 0);
  ExecuteAddPeerConnectionJs(pc);

  const string type = "ssrc";
  const string id = "1234";
  StatsUnit local = {FAKE_TIME_STAMP};
  local.values["bitrate"] = "2000";
  local.values["framerate"] = "30";
  StatsUnit remote = {FAKE_TIME_STAMP};
  remote.values["jitter"] = "1";
  remote.values["rtt"] = "20";

  // Add new stats and verify the stats table and graphs.
  ExecuteAndVerifyAddStats(pc, type, id, local, remote);
  VerifyStatsGraph(pc);

  // Update existing stats and verify the stats table and graphs.
  local.values["bitrate"] = "2001";
  local.values["framerate"] = "31";
  ExecuteAndVerifyAddStats(pc, type, id, local, remote);
  VerifyStatsGraph(pc);
}

// Tests that the bandwidth estimation values are drawn on a single graph.
IN_PROC_BROWSER_TEST_F(WebRTCInternalsBrowserTest, BweCompoundGraph) {
  GURL url("chrome://webrtc-internals");
  NavigateToURL(shell(), url);

  PeerConnectionEntry pc(1, 0);
  ExecuteAddPeerConnectionJs(pc);

  StatsUnit local = {FAKE_TIME_STAMP};
  local.values["googAvailableSendBandwidth"] = "1000000";
  local.values["googTargetEncBitrate"] = "1000";
  local.values["googActualEncBitrate"] = "1000000";
  local.values["googRetransmitBitrate"] = "10";
  local.values["googTransmitBitrate"] = "1000000";
  const string stats_type = "bwe";
  const string stats_id = "videobwe";
  ExecuteAndVerifyAddStats(pc, stats_type, stats_id, local, local);

  string graph_id =
      pc.getIdString() + "-" + stats_type + "-" + stats_id + "-bweCompound";
  bool result = false;
  // Verify that the bweCompound graph exists.
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell()->web_contents(),
        "window.domAutomationController.send("
        "   graphViews['" + graph_id + "'] != null)",
        &result));
  EXPECT_TRUE(result);

  // Verify that the bweCompound graph contains multiple dataSeries.
  int count = 0;
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
        shell()->web_contents(),
        "window.domAutomationController.send("
        "   graphViews['" + graph_id + "'].getDataSeriesCount())",
        &count));
  EXPECT_EQ((int)local.values.size(), count);
}

// Tests that the total packet/byte count is converted to count per second,
// and the converted data is drawn.
IN_PROC_BROWSER_TEST_F(WebRTCInternalsBrowserTest, ConvertedGraphs) {
  GURL url("chrome://webrtc-internals");
  NavigateToURL(shell(), url);

  PeerConnectionEntry pc(1, 0);
  ExecuteAddPeerConnectionJs(pc);

  const string stats_type = "s";
  const string stats_id = "1";
  const int num_converted_stats = 4;
  const string stats_names[] =
      {"packetsSent", "bytesSent", "packetsReceived", "bytesReceived"};
  const string converted_names[] =
      {"packetsSentPerSecond", "bitsSentPerSecond",
       "packetsReceivedPerSecond", "bitsReceivedPerSecond"};
  const string first_value = "1000";
  const string second_value = "2000";
  const string converted_values[] = {"1000", "8000", "1000", "8000"};

  // Send the first data point.
  StatsUnit remote = {FAKE_TIME_STAMP};
  StatsUnit local = {FAKE_TIME_STAMP};
  for (int i = 0; i < num_converted_stats; ++i)
    local.values[stats_names[i]] = first_value;

  ExecuteAndVerifyAddStats(pc, stats_type, stats_id, local, remote);

  // Send the second data point at 1000ms after the first data point.
  local.timestamp += 1000;
  for (int i = 0; i < num_converted_stats; ++i)
    local.values[stats_names[i]] = second_value;
  ExecuteAndVerifyAddStats(pc, stats_type, stats_id, local, remote);

  // Verifies the graph data matches converted_values.
  string graph_id_prefix = pc.getIdString() + "-" + stats_type + "-" + stats_id;
  for (int i = 0; i < num_converted_stats; ++i) {
    VerifyGraphDataPoint(
        graph_id_prefix + "-" + converted_names[i], 1, converted_values[i]);
  }
}

}  // namespace content
