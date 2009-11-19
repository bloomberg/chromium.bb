// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#define MAYBE_IgnoreWords_EN_US IgnoreWords_EN_US
#else
#define MAYBE_IgnoreWords_EN_US DISABLED_IgnoreWords_EN_US
#endif

// Tests that words are properly ignored. Currently only enabled on OS X as it
// is the only platform to support ignoring words. Note that in this test, we
// supply a non-zero doc_tag, in order to test that ignored words are matched to
// the correct document.
TEST(PlatformSpellCheckTest, MAYBE_IgnoreWords_EN_US) {
  static const struct {
    // A misspelled word.
    const char* input;
    bool input_result;
  } kTestCases[] = {
    {"teh"},
    {"moer"},
    {"watre"},
    {"noen"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    string16 word(UTF8ToUTF16(kTestCases[i].input));
    std::vector<string16> suggestions;
    size_t input_length = 0;
    if (kTestCases[i].input != NULL) {
      input_length = word.length();
    }

    int doc_tag = SpellCheckerPlatform::GetDocumentTag();
    bool result = SpellCheckerPlatform::CheckSpelling(word, doc_tag);

    // The word should show up as misspelled.
    EXPECT_EQ(kTestCases[i].input_result, result);

    // Ignore the word.
    SpellCheckerPlatform::IgnoreWord(word);

    // Spellcheck again.
    result = SpellCheckerPlatform::CheckSpelling(word, doc_tag);

    // The word should now show up as correctly spelled.
    EXPECT_EQ(!(kTestCases[i].input_result), result);

    // Close the docuemnt. Any words that we had previously ignored should no
    // longer be ignored and thus should show up as misspelled.
    SpellCheckerPlatform::CloseDocumentWithTag(doc_tag);

    // Spellcheck one more time.
    result = SpellCheckerPlatform::CheckSpelling(word, doc_tag);

    // The word should now show be spelled wrong again
    EXPECT_EQ(kTestCases[i].input_result, result);
  }
} // Test IgnoreWords_EN_US

#if defined(OS_MACOSX)
TEST(PlatformSpellCheckTest, SpellCheckSuggestions_EN_US) {
  static const struct {
    // A string to be tested.
    const wchar_t* input;

    // A suggested word that should occur.
    const wchar_t* suggested_word;
  } kTestCases[] = {    // A valid English word with a preceding whitespace
    // We need to have separate test cases here, since hunspell and the OS X
    // spellchecking service occasionally differ on what they consider a valid
    // suggestion for a given word, although these lists could likely be
    // integrated somewhat. The test cases for non-Mac are in
    // chrome/renderer/spellcheck_unittest.cc
    // These words come from the wikipedia page of the most commonly
    // misspelled words in english.
    // (http://en.wikipedia.org/wiki/Commonly_misspelled_words).
    {L"absense", L"absence"},
    {L"acceptible", L"acceptable"},
    {L"accidentaly", L"accidentally"},
    {L"accomodate", L"accommodate"},
    {L"acheive", L"achieve"},
    {L"acknowlege", L"acknowledge"},
    {L"acquaintence", L"acquaintance"},
    {L"aquire", L"acquire"},
    {L"aquit", L"acquit"},
    {L"acrage", L"acreage"},
    {L"adress", L"address"},
    {L"adultary", L"adultery"},
    {L"advertize", L"advertise"},
    {L"adviseable", L"advisable"},
    {L"agression", L"aggression"},
    {L"alchohol", L"alcohol"},
    {L"alege", L"allege"},
    {L"allegaince", L"allegiance"},
    {L"allmost", L"almost"},
    // Ideally, this test should pass. It works in firefox, but not in hunspell
    // or OS X.
    // {L"alot", L"a lot"},
    {L"amatuer", L"amateur"},
    {L"ammend", L"amend"},
    {L"amung", L"among"},
    {L"anually", L"annually"},
    {L"apparant", L"apparent"},
    {L"artic", L"arctic"},
    {L"arguement", L"argument"},
    {L"athiest", L"atheist"},
    {L"athelete", L"athlete"},
    {L"avrage", L"average"},
    {L"awfull", L"awful"},
    {L"ballance", L"balance"},
    {L"basicly", L"basically"},
    {L"becuase", L"because"},
    {L"becomeing", L"becoming"},
    {L"befor", L"before"},
    {L"begining", L"beginning"},
    {L"beleive", L"believe"},
    {L"bellweather", L"bellwether"},
    {L"benifit", L"benefit"},
    {L"bouy", L"buoy"},
    {L"briliant", L"brilliant"},
    {L"burgler", L"burglar"},
    {L"camoflage", L"camouflage"},
    {L"carrer", L"career"},
    {L"carefull", L"careful"},
    {L"Carribean", L"Caribbean"},
    {L"catagory", L"category"},
    {L"cauhgt", L"caught"},
    {L"cieling", L"ceiling"},
    {L"cemetary", L"cemetery"},
    {L"certin", L"certain"},
    {L"changable", L"changeable"},
    {L"cheif", L"chief"},
    {L"citezen", L"citizen"},
    {L"collaegue", L"colleague"},
    {L"colum", L"column"},
    {L"comming", L"coming"},
    {L"commited", L"committed"},
    {L"compitition", L"competition"},
    {L"conceed", L"concede"},
    {L"congradulate", L"congratulate"},
    {L"consciencious", L"conscientious"},
    {L"concious", L"conscious"},
    {L"concensus", L"consensus"},
    {L"contraversy", L"controversy"},
    {L"conveniance", L"convenience"},
    {L"critecize", L"criticize"},
    {L"dacquiri", L"daiquiri"},
    {L"decieve", L"deceive"},
    {L"dicide", L"decide"},
    {L"definate", L"definite"},
    {L"definitly", L"definitely"},
    {L"deposite", L"deposit"},
    {L"desparate", L"desperate"},
    {L"develope", L"develop"},
    {L"diffrence", L"difference"},
    {L"dilema", L"dilemma"},
    {L"disapear", L"disappear"},
    {L"disapoint", L"disappoint"},
    {L"disasterous", L"disastrous"},
    {L"disipline", L"discipline"},
    {L"drunkeness", L"drunkenness"},
    {L"dumbell", L"dumbbell"},
    {L"durring", L"during"},
    {L"easely", L"easily"},
    {L"eigth", L"eight"},
    {L"embarass", L"embarrass"},
    {L"enviroment", L"environment"},
    {L"equiped", L"equipped"},
    {L"equiptment", L"equipment"},
    {L"exagerate", L"exaggerate"},
    {L"excede", L"exceed"},
    {L"exellent", L"excellent"},
    {L"exsept", L"except"},
    {L"exercize", L"exercise"},
    {L"exilerate", L"exhilarate"},
    {L"existance", L"existence"},
    {L"experiance", L"experience"},
    {L"experament", L"experiment"},
    {L"explaination", L"explanation"},
    {L"extreem", L"extreme"},
    {L"familier", L"familiar"},
    {L"facinating", L"fascinating"},
    {L"firey", L"fiery"},
    {L"finaly", L"finally"},
    {L"flourescent", L"fluorescent"},
    {L"foriegn", L"foreign"},
    {L"fourty", L"forty"},
    {L"foreward", L"forward"},
    {L"freind", L"friend"},
    {L"fullfil", L"fulfill"},
    {L"fundemental", L"fundamental"},
    {L"guage", L"gauge"},
    {L"generaly", L"generally"},
    {L"goverment", L"government"},
    {L"grammer", L"grammar"},
    {L"gratefull", L"grateful"},
    {L"garantee", L"guarantee"},
    {L"guidence", L"guidance"},
    {L"happyness", L"happiness"},
    {L"harrass", L"harass"},
    {L"heighth", L"height"},
    {L"heirarchy", L"hierarchy"},
    {L"humerous", L"humorous"},
    {L"hygene", L"hygiene"},
    {L"hipocrit", L"hypocrite"},
    {L"idenity", L"identity"},
    {L"ignorence", L"ignorance"},
    {L"imaginery", L"imaginary"},
    {L"immitate", L"imitate"},
    {L"immitation", L"imitation"},
    {L"imediately", L"immediately"},
    {L"incidently", L"incidentally"},
    {L"independant", L"independent"},
    {L"indispensible", L"indispensable"},
    {L"innoculate", L"inoculate"},
    {L"inteligence", L"intelligence"},
    {L"intresting", L"interesting"},
    {L"interuption", L"interruption"},
    {L"irrelevent", L"irrelevant"},
    {L"irritible", L"irritable"},
    {L"iland", L"island"},
    {L"jellous", L"jealous"},
    {L"knowlege", L"knowledge"},
    {L"labratory", L"laboratory"},
    {L"liesure", L"leisure"},
    {L"lenght", L"length"},
    {L"liason", L"liaison"},
    {L"libary", L"library"},
    {L"lisence", L"license"},
    {L"lonelyness", L"loneliness"},
    {L"lieing", L"lying"},
    {L"maintenence", L"maintenance"},
    {L"manuever", L"maneuver"},
    {L"marrige", L"marriage"},
    {L"mathmatics", L"mathematics"},
    {L"medcine", L"medicine"},
    {L"medeval", L"medieval"},
    {L"momento", L"memento"},
    {L"millenium", L"millennium"},
    {L"miniture", L"miniature"},
    {L"minite", L"minute"},
    {L"mischevous", L"mischievous"},
    {L"mispell", L"misspell"},
    // Maybe this one should pass, as it works in hunspell, but not in firefox.
    // {L"misterius", L"mysterious"},
    {L"naturaly", L"naturally"},
    {L"neccessary", L"necessary"},
    {L"neice", L"niece"},
    {L"nieghbor", L"neighbor"},
    {L"nieghbour", L"neighbor"},
    {L"niether", L"neither"},
    {L"noticable", L"noticeable"},
    {L"occassion", L"occasion"},
    {L"occasionaly", L"occasionally"},
    {L"occurrance", L"occurrence"},
    {L"occured", L"occurred"},
    {L"oficial", L"official"},
    {L"offen", L"often"},
    {L"ommision", L"omission"},
    {L"oprate", L"operate"},
    {L"oppurtunity", L"opportunity"},
    {L"orignal", L"original"},
    {L"outragous", L"outrageous"},
    {L"parrallel", L"parallel"},
    {L"parliment", L"parliament"},
    {L"particurly", L"particularly"},
    {L"passtime", L"pastime"},
    {L"peculier", L"peculiar"},
    {L"percieve", L"perceive"},
    {L"pernament", L"permanent"},
    {L"perseverence", L"perseverance"},
    {L"personaly", L"personally"},
    {L"personell", L"personnel"},
    {L"persaude", L"persuade"},
    {L"pichure", L"picture"},
    {L"peice", L"piece"},
    {L"plagerize", L"plagiarize"},
    {L"playright", L"playwright"},
    {L"plesant", L"pleasant"},
    {L"pollitical", L"political"},
    {L"posession", L"possession"},
    {L"potatos", L"potatoes"},
    {L"practicle", L"practical"},
    {L"preceed", L"precede"},
    {L"predjudice", L"prejudice"},
    {L"presance", L"presence"},
    {L"privelege", L"privilege"},
    // This one should probably work. It does in FF and Hunspell.
    // {L"probly", L"probably"},
    {L"proffesional", L"professional"},
    {L"professer", L"professor"},
    {L"promiss", L"promise"},
    {L"pronounciation", L"pronunciation"},
    {L"prufe", L"proof"},
    {L"psycology", L"psychology"},
    {L"publically", L"publicly"},
    {L"quanity", L"quantity"},
    {L"quarentine", L"quarantine"},
    {L"questionaire", L"questionnaire"},
    {L"readible", L"readable"},
    {L"realy", L"really"},
    {L"recieve", L"receive"},
    {L"reciept", L"receipt"},
    {L"reconize", L"recognize"},
    {L"recomend", L"recommend"},
    {L"refered", L"referred"},
    {L"referance", L"reference"},
    {L"relevent", L"relevant"},
    {L"religous", L"religious"},
    {L"repitition", L"repetition"},
    {L"restarant", L"restaurant"},
    {L"rythm", L"rhythm"},
    {L"rediculous", L"ridiculous"},
    {L"sacrefice", L"sacrifice"},
    {L"saftey", L"safety"},
    {L"sissors", L"scissors"},
    {L"secratary", L"secretary"},
    {L"sieze", L"seize"},
    {L"seperate", L"separate"},
    {L"sargent", L"sergeant"},
    {L"shineing", L"shining"},
    {L"similer", L"similar"},
    {L"sinceerly", L"sincerely"},
    {L"speach", L"speech"},
    {L"stoping", L"stopping"},
    {L"strenght", L"strength"},
    {L"succede", L"succeed"},
    {L"succesful", L"successful"},
    {L"supercede", L"supersede"},
    {L"surelly", L"surely"},
    {L"suprise", L"surprise"},
    {L"temperture", L"temperature"},
    {L"temprary", L"temporary"},
    {L"tomatos", L"tomatoes"},
    {L"tommorrow", L"tomorrow"},
    {L"tounge", L"tongue"},
    {L"truely", L"truly"},
    {L"twelth", L"twelfth"},
    {L"tyrany", L"tyranny"},
    {L"underate", L"underrate"},
    {L"untill", L"until"},
    {L"unuseual", L"unusual"},
    {L"upholstry", L"upholstery"},
    {L"usible", L"usable"},
    {L"useing", L"using"},
    {L"usualy", L"usually"},
    {L"vaccuum", L"vacuum"},
    {L"vegatarian", L"vegetarian"},
    {L"vehical", L"vehicle"},
    {L"visious", L"vicious"},
    {L"villege", L"village"},
    {L"wierd", L"weird"},
    {L"wellcome", L"welcome"},
    {L"wellfare", L"welfare"},
    {L"wilfull", L"willful"},
    {L"withold", L"withhold"},
    {L"writting", L"writing"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    std::vector<string16> suggestions;
    size_t input_length = 0;
    if (kTestCases[i].input != NULL) {
      input_length = wcslen(kTestCases[i].input);
    }
    bool result = SpellCheckerPlatform::CheckSpelling(
        WideToUTF16(kTestCases[i].input), 0);
    EXPECT_FALSE(result);

    SpellCheckerPlatform::FillSuggestionList(WideToUTF16(kTestCases[i].input),
                                             &suggestions);

    // Check if the suggested words occur.
    bool suggested_word_is_present = false;
    for (int j=0; j < static_cast<int>(suggestions.size()); j++) {
      if (suggestions.at(j).compare(WideToUTF16(kTestCases[i].suggested_word))
          == 0) {
        suggested_word_is_present = true;
        break;
      }
    }

    EXPECT_TRUE(suggested_word_is_present);
  }
}

#endif  // defined(OS_MACOSX)
