var startTest = parent.startTest || function(){};
var test = parent.test || function(name, fn){ fn(); };
var endTest = parent.endTest || function(){};
var prep = parent.prep || function(fn){ fn(); };
